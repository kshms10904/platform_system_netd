/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/binder_manager.h>

#include "netd_resolv/ResolverEventReporter.h"

using aidl::android::net::metrics::INetdEventListener;

std::shared_ptr<INetdEventListener> ResolverEventReporter::getListener() {
    // It should be initialized only once.
    static ResolverEventReporter reporter;

    return reporter.mListener;
}

ResolverEventReporter::ResolverEventReporter() {
    ndk::SpAIBinder binder = ndk::SpAIBinder(AServiceManager_getService("netd_listener"));
    mListener = INetdEventListener::fromBinder(binder);
}
