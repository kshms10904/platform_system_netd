/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * TrafficControllerTest.cpp - unit tests for TrafficController.cpp
 */

#include <string>
#include <vector>

#include <fcntl.h>
#include <inttypes.h>
#include <linux/inet_diag.h>
#include <linux/sock_diag.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <netdutils/MockSyscalls.h>
#include "netdutils/StatusOr.h"

#include "FirewallController.h"
#include "TrafficController.h"
#include "bpf/BpfUtils.h"

using namespace android::bpf;  // NOLINT(google-build-using-namespace): grandfathered

namespace android {
namespace net {

using netdutils::isOk;
using netdutils::StatusOr;

constexpr int TEST_MAP_SIZE = 10;
constexpr uid_t TEST_UID = 10086;
constexpr uid_t TEST_UID2 = 54321;
constexpr uid_t TEST_UID3 = 98765;
constexpr uint32_t TEST_TAG = 42;
constexpr uint32_t TEST_COUNTERSET = 1;
constexpr uint32_t DEFAULT_COUNTERSET = 0;

class TrafficControllerTest : public ::testing::Test {
  protected:
    TrafficControllerTest() {}
    TrafficController mTc;
    BpfMap<uint64_t, UidTag> mFakeCookieTagMap;
    BpfMap<uint32_t, uint8_t> mFakeUidCounterSetMap;
    BpfMap<uint32_t, StatsValue> mFakeAppUidStatsMap;
    BpfMap<StatsKey, StatsValue> mFakeUidStatsMap;
    BpfMap<StatsKey, StatsValue> mFakeTagStatsMap;
    BpfMap<uint32_t, uint8_t> mFakeConfigurationMap;
    BpfMap<uint32_t, uint8_t> mFakeUidOwnerMap;
    BpfMap<uint32_t, uint8_t> mFakeUidPermissionMap;

    void SetUp() {
        std::lock_guard ownerGuard(mTc.mOwnerMatchMutex);
        SKIP_IF_BPF_NOT_SUPPORTED;

        mFakeCookieTagMap.reset(createMap(BPF_MAP_TYPE_HASH, sizeof(uint64_t),
                                          sizeof(struct UidTag), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeCookieTagMap.isValid());

        mFakeUidCounterSetMap.reset(
            createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint8_t), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeUidCounterSetMap.isValid());

        mFakeAppUidStatsMap.reset(createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t),
                                            sizeof(struct StatsValue), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeAppUidStatsMap.isValid());

        mFakeUidStatsMap.reset(createMap(BPF_MAP_TYPE_HASH, sizeof(struct StatsKey),
                                         sizeof(struct StatsValue), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeUidStatsMap.isValid());

        mFakeTagStatsMap.reset(createMap(BPF_MAP_TYPE_HASH, sizeof(struct StatsKey),
                                         sizeof(struct StatsValue), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeTagStatsMap.isValid());

        mFakeConfigurationMap.reset(
                createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint8_t), 1, 0));
        ASSERT_TRUE(mFakeConfigurationMap.isValid());

        mFakeUidOwnerMap.reset(
                createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint8_t), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeUidOwnerMap.isValid());
        mFakeUidPermissionMap.reset(
                createMap(BPF_MAP_TYPE_HASH, sizeof(uint32_t), sizeof(uint8_t), TEST_MAP_SIZE, 0));
        ASSERT_TRUE(mFakeUidPermissionMap.isValid());
        // Make sure trafficController use the eBPF code path.
        mTc.ebpfSupported = true;

        mTc.mCookieTagMap.reset(mFakeCookieTagMap.getMap());
        mTc.mUidCounterSetMap.reset(mFakeUidCounterSetMap.getMap());
        mTc.mAppUidStatsMap.reset(mFakeAppUidStatsMap.getMap());
        mTc.mStatsMapA.reset(mFakeUidStatsMap.getMap());
        mTc.mStatsMapB.reset(mFakeTagStatsMap.getMap());
        mTc.mConfigurationMap.reset(mFakeConfigurationMap.getMap());
        mTc.mUidOwnerMap.reset(mFakeUidOwnerMap.getMap());
        mTc.mUidPermissionMap.reset(mFakeUidPermissionMap.getMap());
        mTc.mPrivilegedUser.clear();
    }

    int setUpSocketAndTag(int protocol, uint64_t* cookie, uint32_t tag, uid_t uid,
                          uid_t callingUid) {
        int sock = socket(protocol, SOCK_STREAM | SOCK_CLOEXEC, 0);
        EXPECT_LE(0, sock);
        *cookie = getSocketCookie(sock);
        EXPECT_NE(NONEXISTENT_COOKIE, *cookie);
        EXPECT_EQ(0, mTc.tagSocket(sock, tag, uid, callingUid));
        return sock;
    }

    void expectUidTag(uint64_t cookie, uid_t uid, uint32_t tag) {
        StatusOr<UidTag> tagResult = mFakeCookieTagMap.readValue(cookie);
        EXPECT_TRUE(isOk(tagResult));
        EXPECT_EQ(uid, tagResult.value().uid);
        EXPECT_EQ(tag, tagResult.value().tag);
    }

    void expectNoTag(uint64_t cookie) { EXPECT_FALSE(isOk(mFakeCookieTagMap.readValue(cookie))); }

    void populateFakeStats(uint64_t cookie, uint32_t uid, uint32_t tag, StatsKey* key) {
        UidTag cookieMapkey = {.uid = (uint32_t)uid, .tag = tag};
        EXPECT_TRUE(isOk(mFakeCookieTagMap.writeValue(cookie, cookieMapkey, BPF_ANY)));
        *key = {.uid = uid, .tag = tag, .counterSet = TEST_COUNTERSET, .ifaceIndex = 1};
        StatsValue statsMapValue = {.rxPackets = 1, .rxBytes = 100};
        uint8_t counterSet = TEST_COUNTERSET;
        EXPECT_TRUE(isOk(mFakeUidCounterSetMap.writeValue(uid, counterSet, BPF_ANY)));
        EXPECT_TRUE(isOk(mFakeTagStatsMap.writeValue(*key, statsMapValue, BPF_ANY)));
        key->tag = 0;
        EXPECT_TRUE(isOk(mFakeUidStatsMap.writeValue(*key, statsMapValue, BPF_ANY)));
        EXPECT_TRUE(isOk(mFakeAppUidStatsMap.writeValue(uid, statsMapValue, BPF_ANY)));
        // put tag information back to statsKey
        key->tag = tag;
    }

    void checkUidOwnerRuleForChain(ChildChain chain, UidOwnerMatchType match) {
        uint32_t uid = TEST_UID;
        EXPECT_EQ(0, mTc.changeUidOwnerRule(chain, uid, DENY, BLACKLIST));
        StatusOr<uint8_t> value = mFakeUidOwnerMap.readValue(uid);
        EXPECT_TRUE(isOk(value));
        EXPECT_TRUE(value.value() & match);

        uid = TEST_UID2;
        EXPECT_EQ(0, mTc.changeUidOwnerRule(chain, uid, ALLOW, WHITELIST));
        value = mFakeUidOwnerMap.readValue(uid);
        EXPECT_TRUE(isOk(value));
        EXPECT_TRUE(value.value() & match);

        EXPECT_EQ(0, mTc.changeUidOwnerRule(chain, uid, DENY, WHITELIST));
        value = mFakeUidOwnerMap.readValue(uid);
        EXPECT_FALSE(isOk(value));
        EXPECT_EQ(ENOENT, value.status().code());

        uid = TEST_UID;
        EXPECT_EQ(0, mTc.changeUidOwnerRule(chain, uid, ALLOW, BLACKLIST));
        value = mFakeUidOwnerMap.readValue(uid);
        EXPECT_FALSE(isOk(value));
        EXPECT_EQ(ENOENT, value.status().code());

        uid = TEST_UID3;
        EXPECT_EQ(-ENOENT, mTc.changeUidOwnerRule(chain, uid, ALLOW, BLACKLIST));
        value = mFakeUidOwnerMap.readValue(uid);
        EXPECT_FALSE(isOk(value));
        EXPECT_EQ(ENOENT, value.status().code());
    }

    void checkEachUidValue(const std::vector<int32_t>& uids, UidOwnerMatchType match) {
        for (uint32_t uid : uids) {
            StatusOr<uint8_t> value = mFakeUidOwnerMap.readValue(uid);
            EXPECT_TRUE(isOk(value));
            EXPECT_TRUE(value.value() & match);
        }
        std::set<uint32_t> uidSet(uids.begin(), uids.end());
        const auto checkNoOtherUid = [&uidSet](const int32_t& key,
                                               const BpfMap<uint32_t, uint8_t>&) {
            EXPECT_NE(uidSet.end(), uidSet.find(key));
            return netdutils::status::ok;
        };
        EXPECT_TRUE(isOk(mFakeUidOwnerMap.iterate(checkNoOtherUid)));
    }

    void checkUidMapReplace(const std::string& name, const std::vector<int32_t>& uids,
                            UidOwnerMatchType match) {
        bool isWhitelist = true;
        EXPECT_EQ(0, mTc.replaceUidOwnerMap(name, isWhitelist, uids));
        checkEachUidValue(uids, match);

        isWhitelist = false;
        EXPECT_EQ(0, mTc.replaceUidOwnerMap(name, isWhitelist, uids));
        checkEachUidValue(uids, match);
    }
    void expectUidOwnerMapValues(const std::vector<std::string>& appStrUids,
                                 uint8_t expectedValue) {
        for (const std::string& strUid : appStrUids) {
            uint32_t uid = stoi(strUid);
            StatusOr<uint8_t> value = mFakeUidOwnerMap.readValue(uid);
            EXPECT_TRUE(isOk(value));
            EXPECT_EQ(expectedValue, value.value()) <<
                "Expected value for UID " << uid << " to be " << expectedValue <<
                ", but was " << value.value();
        }
    }

    void expectMapEmpty(BpfMap<uint64_t, UidTag>& map) {
        auto isEmpty = map.isEmpty();
        EXPECT_TRUE(isOk(isEmpty));
        EXPECT_TRUE(isEmpty.value());
    }

    void expectMapEmpty(BpfMap<uint32_t, uint8_t>& map) {
        auto isEmpty = map.isEmpty();
        ASSERT_TRUE(isOk(isEmpty));
        ASSERT_TRUE(isEmpty.value());
    }

    void expectUidPermissionMapValues(const std::vector<uid_t>& appUids, uint8_t expectedValue) {
        for (uid_t uid : appUids) {
            StatusOr<uint8_t> value = mFakeUidPermissionMap.readValue(uid);
            EXPECT_TRUE(isOk(value));
            EXPECT_EQ(expectedValue, value.value())
                    << "Expected value for UID " << uid << " to be " << expectedValue
                    << ", but was " << value.value();
        }
    }

    void expectPrivilegedUserSet(const std::vector<uid_t>& appUids) {
        EXPECT_EQ(appUids.size(), mTc.mPrivilegedUser.size());
        for (uid_t uid : appUids) {
            EXPECT_NE(mTc.mPrivilegedUser.end(), mTc.mPrivilegedUser.find(uid));
        }
    }

    void expectPrivilegedUserSetEmpty() { EXPECT_TRUE(mTc.mPrivilegedUser.empty()); }

    void addPrivilegedUid(uid_t uid) {
        std::vector privilegedUid = {uid};
        mTc.setPermissionForUids(INetd::PERMISSION_UPDATE_DEVICE_STATS, privilegedUid);
    }

    void removePrivilegedUid(uid_t uid) {
        std::vector privilegedUid = {uid};
        mTc.setPermissionForUids(INetd::NO_PERMISSIONS, privilegedUid);
    }

    void expectFakeStatsUnchanged(uint64_t cookie, uint32_t tag, uint32_t uid,
                                  StatsKey tagStatsMapKey) {
        StatusOr<UidTag> cookieMapResult = mFakeCookieTagMap.readValue(cookie);
        EXPECT_TRUE(isOk(cookieMapResult));
        EXPECT_EQ(uid, cookieMapResult.value().uid);
        EXPECT_EQ(tag, cookieMapResult.value().tag);
        StatusOr<uint8_t> counterSetResult = mFakeUidCounterSetMap.readValue(uid);
        EXPECT_TRUE(isOk(counterSetResult));
        EXPECT_EQ(TEST_COUNTERSET, counterSetResult.value());
        StatusOr<StatsValue> statsMapResult = mFakeTagStatsMap.readValue(tagStatsMapKey);
        EXPECT_TRUE(isOk(statsMapResult));
        EXPECT_EQ((uint64_t)1, statsMapResult.value().rxPackets);
        EXPECT_EQ((uint64_t)100, statsMapResult.value().rxBytes);
        tagStatsMapKey.tag = 0;
        statsMapResult = mFakeUidStatsMap.readValue(tagStatsMapKey);
        EXPECT_TRUE(isOk(statsMapResult));
        EXPECT_EQ((uint64_t)1, statsMapResult.value().rxPackets);
        EXPECT_EQ((uint64_t)100, statsMapResult.value().rxBytes);
        auto appStatsResult = mFakeAppUidStatsMap.readValue(uid);
        EXPECT_TRUE(isOk(appStatsResult));
        EXPECT_EQ((uint64_t)1, appStatsResult.value().rxPackets);
        EXPECT_EQ((uint64_t)100, appStatsResult.value().rxBytes);
    }
};

TEST_F(TrafficControllerTest, TestTagSocketV4) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uint64_t sockCookie;
    int v4socket = setUpSocketAndTag(AF_INET, &sockCookie, TEST_TAG, TEST_UID, TEST_UID);
    expectUidTag(sockCookie, TEST_UID, TEST_TAG);
    ASSERT_EQ(0, mTc.untagSocket(v4socket));
    expectNoTag(sockCookie);
    expectMapEmpty(mFakeCookieTagMap);
}

TEST_F(TrafficControllerTest, TestReTagSocket) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uint64_t sockCookie;
    int v4socket = setUpSocketAndTag(AF_INET, &sockCookie, TEST_TAG, TEST_UID, TEST_UID);
    expectUidTag(sockCookie, TEST_UID, TEST_TAG);
    ASSERT_EQ(0, mTc.tagSocket(v4socket, TEST_TAG + 1, TEST_UID + 1, TEST_UID + 1));
    expectUidTag(sockCookie, TEST_UID + 1, TEST_TAG + 1);
}

TEST_F(TrafficControllerTest, TestTagTwoSockets) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uint64_t sockCookie1;
    uint64_t sockCookie2;
    int v4socket1 = setUpSocketAndTag(AF_INET, &sockCookie1, TEST_TAG, TEST_UID, TEST_UID);
    setUpSocketAndTag(AF_INET, &sockCookie2, TEST_TAG, TEST_UID, TEST_UID);
    expectUidTag(sockCookie1, TEST_UID, TEST_TAG);
    expectUidTag(sockCookie2, TEST_UID, TEST_TAG);
    ASSERT_EQ(0, mTc.untagSocket(v4socket1));
    expectNoTag(sockCookie1);
    expectUidTag(sockCookie2, TEST_UID, TEST_TAG);
    ASSERT_FALSE(isOk(mFakeCookieTagMap.getNextKey(sockCookie2)));
}

TEST_F(TrafficControllerTest, TestTagSocketV6) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uint64_t sockCookie;
    int v6socket = setUpSocketAndTag(AF_INET6, &sockCookie, TEST_TAG, TEST_UID, TEST_UID);
    expectUidTag(sockCookie, TEST_UID, TEST_TAG);
    ASSERT_EQ(0, mTc.untagSocket(v6socket));
    expectNoTag(sockCookie);
    expectMapEmpty(mFakeCookieTagMap);
}

TEST_F(TrafficControllerTest, TestTagInvalidSocket) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int invalidSocket = -1;
    ASSERT_GT(0, mTc.tagSocket(invalidSocket, TEST_TAG, TEST_UID, TEST_UID));
    expectMapEmpty(mFakeCookieTagMap);
}

TEST_F(TrafficControllerTest, TestTagSocketWithoutPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int sock = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_NE(-1, sock);
    ASSERT_EQ(-EPERM, mTc.tagSocket(sock, TEST_TAG, TEST_UID, TEST_UID2));
    expectMapEmpty(mFakeCookieTagMap);
}

TEST_F(TrafficControllerTest, TestTagSocketWithPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    // Grant permission to calling uid.
    std::vector<uid_t> callingUid = {TEST_UID2};
    mTc.setPermissionForUids(INetd::PERMISSION_UPDATE_DEVICE_STATS, callingUid);

    // Tag a socket to a different uid other then callingUid.
    uint64_t sockCookie;
    int v6socket = setUpSocketAndTag(AF_INET6, &sockCookie, TEST_TAG, TEST_UID, TEST_UID2);
    expectUidTag(sockCookie, TEST_UID, TEST_TAG);
    EXPECT_EQ(0, mTc.untagSocket(v6socket));
    expectNoTag(sockCookie);
    expectMapEmpty(mFakeCookieTagMap);

    // Clean up the permission
    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, callingUid);
    expectPrivilegedUserSetEmpty();
}

TEST_F(TrafficControllerTest, TestUntagInvalidSocket) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int invalidSocket = -1;
    ASSERT_GT(0, mTc.untagSocket(invalidSocket));
    int v4socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_GT(0, mTc.untagSocket(v4socket));
    expectMapEmpty(mFakeCookieTagMap);
}

TEST_F(TrafficControllerTest, TestSetCounterSet) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uid_t callingUid = TEST_UID2;
    addPrivilegedUid(callingUid);
    ASSERT_EQ(0, mTc.setCounterSet(TEST_COUNTERSET, TEST_UID, callingUid));
    uid_t uid = TEST_UID;
    StatusOr<uint8_t> counterSetResult = mFakeUidCounterSetMap.readValue(uid);
    ASSERT_TRUE(isOk(counterSetResult));
    ASSERT_EQ(TEST_COUNTERSET, counterSetResult.value());
    ASSERT_EQ(0, mTc.setCounterSet(DEFAULT_COUNTERSET, TEST_UID, callingUid));
    ASSERT_FALSE(isOk(mFakeUidCounterSetMap.readValue(uid)));
    expectMapEmpty(mFakeUidCounterSetMap);
}

TEST_F(TrafficControllerTest, TestSetCounterSetWithoutPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    ASSERT_EQ(-EPERM, mTc.setCounterSet(TEST_COUNTERSET, TEST_UID, TEST_UID2));
    uid_t uid = TEST_UID;
    ASSERT_FALSE(isOk(mFakeUidCounterSetMap.readValue(uid)));
    expectMapEmpty(mFakeUidCounterSetMap);
}

TEST_F(TrafficControllerTest, TestSetInvalidCounterSet) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uid_t callingUid = TEST_UID2;
    addPrivilegedUid(callingUid);
    ASSERT_GT(0, mTc.setCounterSet(OVERFLOW_COUNTERSET, TEST_UID, callingUid));
    uid_t uid = TEST_UID;
    ASSERT_FALSE(isOk(mFakeUidCounterSetMap.readValue(uid)));
    expectMapEmpty(mFakeUidCounterSetMap);
}

TEST_F(TrafficControllerTest, TestDeleteTagDataWithoutPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uint64_t cookie = 1;
    uid_t uid = TEST_UID;
    uint32_t tag = TEST_TAG;
    StatsKey tagStatsMapKey;
    populateFakeStats(cookie, uid, tag, &tagStatsMapKey);
    ASSERT_EQ(-EPERM, mTc.deleteTagData(0, TEST_UID, TEST_UID2));

    expectFakeStatsUnchanged(cookie, tag, uid, tagStatsMapKey);
}

TEST_F(TrafficControllerTest, TestDeleteTagData) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uid_t callingUid = TEST_UID2;
    addPrivilegedUid(callingUid);
    uint64_t cookie = 1;
    uid_t uid = TEST_UID;
    uint32_t tag = TEST_TAG;
    StatsKey tagStatsMapKey;
    populateFakeStats(cookie, uid, tag, &tagStatsMapKey);
    ASSERT_EQ(0, mTc.deleteTagData(TEST_TAG, TEST_UID, callingUid));
    ASSERT_FALSE(isOk(mFakeCookieTagMap.readValue(cookie)));
    StatusOr<uint8_t> counterSetResult = mFakeUidCounterSetMap.readValue(uid);
    ASSERT_TRUE(isOk(counterSetResult));
    ASSERT_EQ(TEST_COUNTERSET, counterSetResult.value());
    ASSERT_FALSE(isOk(mFakeTagStatsMap.readValue(tagStatsMapKey)));
    tagStatsMapKey.tag = 0;
    StatusOr<StatsValue> statsMapResult = mFakeUidStatsMap.readValue(tagStatsMapKey);
    ASSERT_TRUE(isOk(statsMapResult));
    ASSERT_EQ((uint64_t)1, statsMapResult.value().rxPackets);
    ASSERT_EQ((uint64_t)100, statsMapResult.value().rxBytes);
    auto appStatsResult = mFakeAppUidStatsMap.readValue(TEST_UID);
    ASSERT_TRUE(isOk(appStatsResult));
    ASSERT_EQ((uint64_t)1, appStatsResult.value().rxPackets);
    ASSERT_EQ((uint64_t)100, appStatsResult.value().rxBytes);
}

TEST_F(TrafficControllerTest, TestDeleteAllUidData) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uid_t callingUid = TEST_UID2;
    addPrivilegedUid(callingUid);
    uint64_t cookie = 1;
    uid_t uid = TEST_UID;
    uint32_t tag = TEST_TAG;
    StatsKey tagStatsMapKey;
    populateFakeStats(cookie, uid, tag, &tagStatsMapKey);
    ASSERT_EQ(0, mTc.deleteTagData(0, TEST_UID, callingUid));
    ASSERT_FALSE(isOk(mFakeCookieTagMap.readValue(cookie)));
    ASSERT_FALSE(isOk(mFakeUidCounterSetMap.readValue(uid)));
    ASSERT_FALSE(isOk(mFakeTagStatsMap.readValue(tagStatsMapKey)));
    tagStatsMapKey.tag = 0;
    ASSERT_FALSE(isOk(mFakeUidStatsMap.readValue(tagStatsMapKey)));
    ASSERT_FALSE(isOk(mFakeAppUidStatsMap.readValue(TEST_UID)));
}

TEST_F(TrafficControllerTest, TestDeleteDataWithTwoTags) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uid_t callingUid = TEST_UID2;
    addPrivilegedUid(callingUid);
    uint64_t cookie1 = 1;
    uint64_t cookie2 = 2;
    uid_t uid = TEST_UID;
    uint32_t tag1 = TEST_TAG;
    uint32_t tag2 = TEST_TAG + 1;
    StatsKey tagStatsMapKey1;
    StatsKey tagStatsMapKey2;
    populateFakeStats(cookie1, uid, tag1, &tagStatsMapKey1);
    populateFakeStats(cookie2, uid, tag2, &tagStatsMapKey2);
    ASSERT_EQ(0, mTc.deleteTagData(TEST_TAG, TEST_UID, callingUid));
    ASSERT_FALSE(isOk(mFakeCookieTagMap.readValue(cookie1)));
    StatusOr<UidTag> cookieMapResult = mFakeCookieTagMap.readValue(cookie2);
    ASSERT_TRUE(isOk(cookieMapResult));
    ASSERT_EQ(TEST_UID, cookieMapResult.value().uid);
    ASSERT_EQ(TEST_TAG + 1, cookieMapResult.value().tag);
    StatusOr<uint8_t> counterSetResult = mFakeUidCounterSetMap.readValue(uid);
    ASSERT_TRUE(isOk(counterSetResult));
    ASSERT_EQ(TEST_COUNTERSET, counterSetResult.value());
    ASSERT_FALSE(isOk(mFakeTagStatsMap.readValue(tagStatsMapKey1)));
    StatusOr<StatsValue> statsMapResult = mFakeTagStatsMap.readValue(tagStatsMapKey2);
    ASSERT_TRUE(isOk(statsMapResult));
    ASSERT_EQ((uint64_t)1, statsMapResult.value().rxPackets);
    ASSERT_EQ((uint64_t)100, statsMapResult.value().rxBytes);
}

TEST_F(TrafficControllerTest, TestDeleteDataWithTwoUids) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uid_t callingUid = TEST_UID2;
    addPrivilegedUid(callingUid);
    uint64_t cookie1 = 1;
    uint64_t cookie2 = 2;
    uid_t uid1 = TEST_UID;
    uid_t uid2 = TEST_UID + 1;
    uint32_t tag = TEST_TAG;
    StatsKey tagStatsMapKey1;
    StatsKey tagStatsMapKey2;
    populateFakeStats(cookie1, uid1, tag, &tagStatsMapKey1);
    populateFakeStats(cookie2, uid2, tag, &tagStatsMapKey2);

    // Delete the stats of one of the uid. Check if it is properly collected by
    // removedStats.
    ASSERT_EQ(0, mTc.deleteTagData(0, uid2, callingUid));
    ASSERT_FALSE(isOk(mFakeCookieTagMap.readValue(cookie2)));
    StatusOr<uint8_t> counterSetResult = mFakeUidCounterSetMap.readValue(uid1);
    ASSERT_TRUE(isOk(counterSetResult));
    ASSERT_EQ(TEST_COUNTERSET, counterSetResult.value());
    ASSERT_FALSE(isOk(mFakeUidCounterSetMap.readValue(uid2)));
    ASSERT_FALSE(isOk(mFakeTagStatsMap.readValue(tagStatsMapKey2)));
    tagStatsMapKey2.tag = 0;
    ASSERT_FALSE(isOk(mFakeUidStatsMap.readValue(tagStatsMapKey2)));
    ASSERT_FALSE(isOk(mFakeAppUidStatsMap.readValue(uid2)));
    tagStatsMapKey1.tag = 0;
    StatusOr<StatsValue> statsMapResult = mFakeUidStatsMap.readValue(tagStatsMapKey1);
    ASSERT_TRUE(isOk(statsMapResult));
    ASSERT_EQ((uint64_t)1, statsMapResult.value().rxPackets);
    ASSERT_EQ((uint64_t)100, statsMapResult.value().rxBytes);
    auto appStatsResult = mFakeAppUidStatsMap.readValue(uid1);
    ASSERT_TRUE(isOk(appStatsResult));
    ASSERT_EQ((uint64_t)1, appStatsResult.value().rxPackets);
    ASSERT_EQ((uint64_t)100, appStatsResult.value().rxBytes);

    // Delete the stats of the other uid.
    ASSERT_EQ(0, mTc.deleteTagData(0, uid1, callingUid));
    ASSERT_FALSE(isOk(mFakeUidStatsMap.readValue(tagStatsMapKey1)));
    ASSERT_FALSE(isOk(mFakeAppUidStatsMap.readValue(uid1)));
}

TEST_F(TrafficControllerTest, TestUpdateOwnerMapEntry) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    uint32_t uid = TEST_UID;
    ASSERT_TRUE(isOk(mTc.updateOwnerMapEntry(STANDBY_MATCH, uid, DENY, BLACKLIST)));
    StatusOr<uint8_t> value = mFakeUidOwnerMap.readValue(uid);
    ASSERT_TRUE(isOk(value));
    ASSERT_TRUE(value.value() & STANDBY_MATCH);

    ASSERT_TRUE(isOk(mTc.updateOwnerMapEntry(DOZABLE_MATCH, uid, ALLOW, WHITELIST)));
    value = mFakeUidOwnerMap.readValue(uid);
    ASSERT_TRUE(isOk(value));
    ASSERT_TRUE(value.value() & DOZABLE_MATCH);

    ASSERT_TRUE(isOk(mTc.updateOwnerMapEntry(DOZABLE_MATCH, uid, DENY, WHITELIST)));
    value = mFakeUidOwnerMap.readValue(uid);
    ASSERT_TRUE(isOk(value));
    ASSERT_FALSE(value.value() & DOZABLE_MATCH);

    ASSERT_TRUE(isOk(mTc.updateOwnerMapEntry(STANDBY_MATCH, uid, ALLOW, BLACKLIST)));
    ASSERT_FALSE(isOk(mFakeUidOwnerMap.readValue(uid)));

    uid = TEST_UID2;
    ASSERT_FALSE(isOk(mTc.updateOwnerMapEntry(STANDBY_MATCH, uid, ALLOW, BLACKLIST)));
    ASSERT_FALSE(isOk(mFakeUidOwnerMap.readValue(uid)));
}

TEST_F(TrafficControllerTest, TestChangeUidOwnerRule) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    checkUidOwnerRuleForChain(DOZABLE, DOZABLE_MATCH);
    checkUidOwnerRuleForChain(STANDBY, STANDBY_MATCH);
    checkUidOwnerRuleForChain(POWERSAVE, POWERSAVE_MATCH);
    ASSERT_EQ(-EINVAL, mTc.changeUidOwnerRule(NONE, TEST_UID, ALLOW, WHITELIST));
    ASSERT_EQ(-EINVAL, mTc.changeUidOwnerRule(INVALID_CHAIN, TEST_UID, ALLOW, WHITELIST));
}

TEST_F(TrafficControllerTest, TestReplaceUidOwnerMap) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<int32_t> uids = {TEST_UID, TEST_UID2, TEST_UID3};
    checkUidMapReplace("fw_dozable", uids, DOZABLE_MATCH);
    checkUidMapReplace("fw_standby", uids, STANDBY_MATCH);
    checkUidMapReplace("fw_powersave", uids, POWERSAVE_MATCH);
    ASSERT_EQ(-EINVAL, mTc.replaceUidOwnerMap("unknow", true, uids));
}

TEST_F(TrafficControllerTest, TestReplaceSameChain) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<int32_t> uids = {TEST_UID, TEST_UID2, TEST_UID3};
    checkUidMapReplace("fw_dozable", uids, DOZABLE_MATCH);
    std::vector<int32_t> newUids = {TEST_UID2, TEST_UID3};
    checkUidMapReplace("fw_dozable", newUids, DOZABLE_MATCH);
}

TEST_F(TrafficControllerTest, TestBlacklistUidMatch) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<std::string> appStrUids = {"1000", "1001", "10012"};
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReject,
                                           BandwidthController::IptOpInsert)));
    expectUidOwnerMapValues(appStrUids, PENALTY_BOX_MATCH);
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReject,
                                           BandwidthController::IptOpDelete)));
    expectMapEmpty(mFakeUidOwnerMap);
}

TEST_F(TrafficControllerTest, TestWhitelistUidMatch) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<std::string> appStrUids = {"1000", "1001", "10012"};
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReturn,
                                           BandwidthController::IptOpInsert)));
    expectUidOwnerMapValues(appStrUids, HAPPY_BOX_MATCH);
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReturn,
                                           BandwidthController::IptOpDelete)));
    expectMapEmpty(mFakeUidOwnerMap);
}

TEST_F(TrafficControllerTest, TestReplaceMatchUid) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<std::string> appStrUids = {"1000", "1001", "10012"};
    // Add appStrUids to the blacklist and expect that their values are all PENALTY_BOX_MATCH.
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReject,
                                           BandwidthController::IptOpInsert)));
    expectUidOwnerMapValues(appStrUids, PENALTY_BOX_MATCH);

    // Add the same UIDs to the whitelist and expect that we get PENALTY_BOX_MATCH |
    // HAPPY_BOX_MATCH.
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReturn,
                                           BandwidthController::IptOpInsert)));
    expectUidOwnerMapValues(appStrUids, HAPPY_BOX_MATCH | PENALTY_BOX_MATCH);

    // Remove the same UIDs from the whitelist and check the PENALTY_BOX_MATCH is still there.
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReturn,
                                           BandwidthController::IptOpDelete)));
    expectUidOwnerMapValues(appStrUids, PENALTY_BOX_MATCH);

    // Remove the same UIDs from the blacklist and check the map is empty.
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReject,
                                           BandwidthController::IptOpDelete)));
    ASSERT_FALSE(isOk(mFakeUidOwnerMap.getFirstKey()));
}

TEST_F(TrafficControllerTest, TestDeleteWrongMatchSilentlyFails) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<std::string> appStrUids = {"1000", "1001", "10012"};
    // If the uid does not exist in the map, trying to delete a rule about it will fail.
    ASSERT_FALSE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReturn,
                                            BandwidthController::IptOpDelete)));
    expectMapEmpty(mFakeUidOwnerMap);

    // Add blacklist rules for appStrUids.
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReturn,
                                           BandwidthController::IptOpInsert)));
    expectUidOwnerMapValues(appStrUids, HAPPY_BOX_MATCH);

    // Delete (non-existent) blacklist rules for appStrUids, and check that this silently does
    // nothing if the uid is in the map but does not have blacklist match. This is required because
    // NetworkManagementService will try to remove a uid from blacklist after adding it to the
    // whitelist and if the remove fails it will not update the uid status.
    ASSERT_TRUE(isOk(mTc.updateUidOwnerMap(appStrUids, BandwidthController::IptJumpReject,
                                           BandwidthController::IptOpDelete)));
    expectUidOwnerMapValues(appStrUids, HAPPY_BOX_MATCH);
}

TEST_F(TrafficControllerTest, TestGrantInternetPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<uid_t> appUids = {TEST_UID, TEST_UID2, TEST_UID3};

    mTc.setPermissionForUids(INetd::PERMISSION_INTERNET, appUids);
    expectUidPermissionMapValues(appUids, INetd::PERMISSION_INTERNET);
    expectPrivilegedUserSetEmpty();

    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, appUids);
    expectMapEmpty(mFakeUidPermissionMap);
}

TEST_F(TrafficControllerTest, TestRevokeInternetPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<uid_t> appUids = {TEST_UID, TEST_UID2, TEST_UID3};

    mTc.setPermissionForUids(INetd::PERMISSION_INTERNET, appUids);
    expectUidPermissionMapValues(appUids, INetd::PERMISSION_INTERNET);

    std::vector<uid_t> uidToRemove = {TEST_UID};
    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, uidToRemove);

    std::vector<uid_t> uidRemain = {TEST_UID3, TEST_UID2};
    expectUidPermissionMapValues(uidRemain, INetd::PERMISSION_INTERNET);

    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, uidRemain);
    expectMapEmpty(mFakeUidPermissionMap);
}

TEST_F(TrafficControllerTest, TestGrantUpdateStatsPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<uid_t> appUids = {TEST_UID, TEST_UID2, TEST_UID3};

    mTc.setPermissionForUids(INetd::PERMISSION_UPDATE_DEVICE_STATS, appUids);
    expectMapEmpty(mFakeUidPermissionMap);
    expectPrivilegedUserSet(appUids);

    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, appUids);
    expectPrivilegedUserSetEmpty();
}

TEST_F(TrafficControllerTest, TestRevokeUpdateStatsPermission) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<uid_t> appUids = {TEST_UID, TEST_UID2, TEST_UID3};

    mTc.setPermissionForUids(INetd::PERMISSION_UPDATE_DEVICE_STATS, appUids);
    expectPrivilegedUserSet(appUids);

    std::vector<uid_t> uidToRemove = {TEST_UID};
    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, uidToRemove);

    std::vector<uid_t> uidRemain = {TEST_UID3, TEST_UID2};
    expectPrivilegedUserSet(uidRemain);

    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, uidRemain);
    expectPrivilegedUserSetEmpty();
}

TEST_F(TrafficControllerTest, TestGrantWrongPermissionSlientlyFail) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<uid_t> appUids = {TEST_UID, TEST_UID2, TEST_UID3};

    mTc.setPermissionForUids(INetd::PERMISSION_NONE, appUids);
    expectPrivilegedUserSetEmpty();
    expectMapEmpty(mFakeUidPermissionMap);
}

TEST_F(TrafficControllerTest, TestGrantDuplicatePermissionSlientlyFail) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    std::vector<uid_t> appUids = {TEST_UID, TEST_UID2, TEST_UID3};

    mTc.setPermissionForUids(INetd::PERMISSION_INTERNET, appUids);
    expectUidPermissionMapValues(appUids, INetd::PERMISSION_INTERNET);

    std::vector<uid_t> uidToAdd = {TEST_UID};
    mTc.setPermissionForUids(INetd::PERMISSION_INTERNET, uidToAdd);

    expectUidPermissionMapValues(appUids, INetd::PERMISSION_INTERNET);

    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, appUids);
    expectMapEmpty(mFakeUidPermissionMap);

    mTc.setPermissionForUids(INetd::PERMISSION_UPDATE_DEVICE_STATS, appUids);
    expectPrivilegedUserSet(appUids);

    mTc.setPermissionForUids(INetd::PERMISSION_UPDATE_DEVICE_STATS, uidToAdd);
    expectPrivilegedUserSet(appUids);

    mTc.setPermissionForUids(INetd::NO_PERMISSIONS, appUids);
    expectPrivilegedUserSetEmpty();
}

}  // namespace net
}  // namespace android
