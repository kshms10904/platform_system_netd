/**
 * Copyright (c) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.net;

import android.net.UidRangeParcel;
import android.net.TetherStatsParcel;
import android.net.INetdUnsolicitedEventListener;
import android.net.InterfaceConfigurationParcel;

/** {@hide} */
interface INetd {
    /**
     * Returns true if the service is responding.
     */
    boolean isAlive();

    /**
     * Replaces the contents of the specified UID-based firewall chain.
     *
     * The chain may be a whitelist chain or a blacklist chain. A blacklist chain contains DROP
     * rules for the specified UIDs and a RETURN rule at the end. A whitelist chain contains RETURN
     * rules for the system UID range (0 to {@code UID_APP} - 1), RETURN rules for for the specified
     * UIDs, and a DROP rule at the end. The chain will be created if it does not exist.
     *
     * @param chainName The name of the chain to replace.
     * @param isWhitelist Whether this is a whitelist or blacklist chain.
     * @param uids The list of UIDs to allow/deny.
     * @return true if the chain was successfully replaced, false otherwise.
     */
    boolean firewallReplaceUidChain(in @utf8InCpp String chainName,
                                    boolean isWhitelist,
                                    in int[] uids);

    /**
     * Enables or disables data saver mode on costly network interfaces.
     *
     * - When disabled, all packets to/from apps in the penalty box chain are rejected on costly
     *   interfaces. Traffic to/from other apps or on other network interfaces is allowed.
     * - When enabled, only apps that are in the happy box chain and not in the penalty box chain
     *   are allowed network connectivity on costly interfaces. All other packets on these
     *   interfaces are rejected. The happy box chain always contains all system UIDs; to disallow
     *   traffic from system UIDs, place them in the penalty box chain.
     *
     * By default, data saver mode is disabled. This command has no effect but might still return an
     * error) if {@code enable} is the same as the current value.
     *
     * @param enable whether to enable or disable data saver mode.
     * @return true if the if the operation was successful, false otherwise.
     */
    boolean bandwidthEnableDataSaver(boolean enable);

    /**
     * Creates a physical network (i.e., one containing physical interfaces.
     *
     * @param netId the networkId to create.
     * @param permission the permission necessary to use the network. Must be one of
     *         PERMISSION_NONE/PERMISSION_NETWORK/PERMISSION_SYSTEM.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkCreatePhysical(int netId, int permission);

    /**
     * Creates a VPN network.
     *
     * @param netId the network to create.
     * @param secure whether unprivileged apps are allowed to bypass the VPN.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkCreateVpn(int netId, boolean secure);

    /**
     * Destroys a network. Any interfaces added to the network are removed, and the network ceases
     * to be the default network.
     *
     * @param netId the network to destroy.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkDestroy(int netId);

    /**
     * Adds an interface to a network. The interface must not be assigned to any network, including
     * the specified network.
     *
     * @param netId the network to add the interface to.
     * @param interface the name of the interface to add.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkAddInterface(int netId, in @utf8InCpp String iface);

    /**
     * Adds an interface to a network. The interface must be assigned to the specified network.
     *
     * @param netId the network to remove the interface from.
     * @param interface the name of the interface to remove.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkRemoveInterface(int netId, in @utf8InCpp String iface);

    /**
     * Adds the specified UID ranges to the specified network. The network must be a VPN. Traffic
     * from the UID ranges will be routed through the VPN.
     *
     * @param netId the network ID of the network to add the ranges to.
     * @param uidRanges a set of non-overlapping, contiguous ranges of UIDs to add. The ranges
     *        must not overlap with existing ranges routed to this network.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkAddUidRanges(int netId, in UidRangeParcel[] uidRanges);

    /**
     * Adds the specified UID ranges to the specified network. The network must be a VPN. Traffic
     * from the UID ranges will no longer be routed through the VPN.
     *
     * @param netId the network ID of the network to remove the ranges from.
     * @param uidRanges a set of non-overlapping, contiguous ranges of UIDs to add. The ranges
     *        must already be routed to this network.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkRemoveUidRanges(int netId, in UidRangeParcel[] uidRanges);

    /**
     * Adds or removes one rule for each supplied UID range to prohibit all network activity outside
     * of secure VPN.
     *
     * When a UID is covered by one of these rules, traffic sent through any socket that is not
     * protected or explicitly overriden by the system will be rejected. The kernel will respond
     * with an ICMP prohibit message.
     *
     * Initially, there are no such rules. Any rules that are added will only last until the next
     * restart of netd or the device.
     *
     * @param add {@code true} if the specified UID ranges should be denied access to any network
     *        which is not secure VPN by adding rules, {@code false} to remove existing rules.
     * @param uidRanges a set of non-overlapping, contiguous ranges of UIDs to which to apply or
     *        remove this restriction.
     *        <p> Added rules should not overlap with existing rules. Likewise, removed rules should
     *        each correspond to an existing rule.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void networkRejectNonSecureVpn(boolean add, in UidRangeParcel[] uidRanges);

    /**
     * Administratively closes sockets belonging to the specified UIDs.
     */
    void socketDestroy(in UidRangeParcel[] uidRanges, in int[] exemptUids);

    // Array indices for resolver parameters.
    const int RESOLVER_PARAMS_SAMPLE_VALIDITY = 0;
    const int RESOLVER_PARAMS_SUCCESS_THRESHOLD = 1;
    const int RESOLVER_PARAMS_MIN_SAMPLES = 2;
    const int RESOLVER_PARAMS_MAX_SAMPLES = 3;
    const int RESOLVER_PARAMS_BASE_TIMEOUT_MSEC = 4;
    const int RESOLVER_PARAMS_COUNT = 5;

    /**
     * Sets the name servers, search domains and resolver params for the given network. Flushes the
     * cache as needed (i.e. when the servers or the number of samples to store changes).
     *
     * @param netId the network ID of the network for which information should be configured.
     * @param servers the DNS servers to configure for the network.
     * @param domains the search domains to configure.
     * @param params the params to set. This array contains RESOLVER_PARAMS_COUNT integers that
     *   encode the contents of Bionic's __res_params struct, i.e. sample_validity is stored at
     *   position RESOLVER_PARAMS_SAMPLE_VALIDITY, etc.
     * @param tlsName The TLS subject name to require for all servers, or empty if there is none.
     * @param tlsServers the DNS servers to configure for strict mode Private DNS.
     * @param tlsFingerprints An array containing TLS public key fingerprints (pins) of which each
     *   server must match at least one, or empty if there are no pinned keys.
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void setResolverConfiguration(int netId, in @utf8InCpp String[] servers,
            in @utf8InCpp String[] domains, in int[] params,
            in @utf8InCpp String tlsName, in @utf8InCpp String[] tlsServers,
            in @utf8InCpp String[] tlsFingerprints);

    // Array indices for resolver stats.
    const int RESOLVER_STATS_SUCCESSES = 0;
    const int RESOLVER_STATS_ERRORS = 1;
    const int RESOLVER_STATS_TIMEOUTS = 2;
    const int RESOLVER_STATS_INTERNAL_ERRORS = 3;
    const int RESOLVER_STATS_RTT_AVG = 4;
    const int RESOLVER_STATS_LAST_SAMPLE_TIME = 5;
    const int RESOLVER_STATS_USABLE = 6;
    const int RESOLVER_STATS_COUNT = 7;

    /**
     * Retrieves the name servers, search domains and resolver stats associated with the given
     * network ID.
     *
     * @param netId the network ID of the network for which information should be retrieved.
     * @param servers the DNS servers that are currently configured for the network.
     * @param domains the search domains currently configured.
     * @param tlsServers the DNS-over-TLS servers that are currently configured for the network.
     * @param params the resolver parameters configured, i.e. the contents of __res_params in order.
     * @param stats the stats for each server in the order specified by RESOLVER_STATS_XXX
     *         constants, serialized as an int array. The contents of this array are the number of
     *         <ul>
     *           <li> successes,
     *           <li> errors,
     *           <li> timeouts,
     *           <li> internal errors,
     *           <li> the RTT average,
     *           <li> the time of the last recorded sample,
     *           <li> and an integer indicating whether the server is usable (1) or broken (0).
     *         </ul>
     *         in this order. For example, the timeout counter for server N is stored at position
     *         RESOLVER_STATS_COUNT*N + RESOLVER_STATS_TIMEOUTS
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     *
     * TODO: Consider replacing stats and params with parcelables.
     */
    void getResolverInfo(int netId, out @utf8InCpp String[] servers,
            out @utf8InCpp String[] domains, out @utf8InCpp String[] tlsServers, out int[] params,
            out int[] stats);

    /**
     * Instruct the tethering DNS server to reevaluated serving interfaces.
     * This is needed to for the DNS server to observe changes in the set
     * of potential listening IP addresses. (Listening on wildcard addresses
     * can turn the device into an open resolver; b/7530468)
     *
     * TODO: Return something richer than just a boolean.
     */
    boolean tetherApplyDnsInterfaces();

    /**
     * Return tethering statistics.
     *
     * @return an array of TetherStatsParcel, where each entry contains the upstream interface
     *         name and its tethering statistics.
     *         There will only ever be one entry for a given interface.
     * @throws ServiceSpecificException in case of failure, with an error code indicating the
     *         cause of the the failure.
     */
    TetherStatsParcel[] tetherGetStats();

    /**
     * Add/Remove and IP address from an interface.
     *
     * @param ifName the interface name
     * @param addrString the IP address to add/remove as a string literal
     * @param prefixLength the prefix length associated with this IP address
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */
    void interfaceAddAddress(in @utf8InCpp String ifName, in @utf8InCpp String addrString,
            int prefixLength);
    void interfaceDelAddress(in @utf8InCpp String ifName, in @utf8InCpp String addrString,
            int prefixLength);

    /**
     * Set and get /proc/sys/net interface configuration parameters.
     *
     * @param ipversion One of IPV4/IPV6 integers, indicating the desired IP version directory.
     * @param which One of CONF/NEIGH integers, indicating the desired parameter category directory.
     * @param ifname The interface name portion of the path; may also be "all" or "default".
     * @param parameter The parameter name portion of the path.
     * @param value The value string to be written into the assembled path.
     *
     * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
     *         unix errno.
     */

    const int IPV4  = 4;
    const int IPV6  = 6;
    const int CONF  = 1;
    const int NEIGH = 2;
    @utf8InCpp String getProcSysNet(int ipversion, int which, in @utf8InCpp String ifname,
            in @utf8InCpp String parameter);
    void setProcSysNet(int ipversion, int which, in @utf8InCpp String ifname,
            in @utf8InCpp String parameter, in @utf8InCpp String value);

   /**
    * Sets owner of socket ParcelFileDescriptor to the new UID, checking to ensure that the caller's
    * uid is that of the old owner's, and that this is a UDP-encap socket
    *
    * @param ParcelFileDescriptor socket Socket file descriptor
    * @param int newUid UID of the new socket fd owner
    */
    void ipSecSetEncapSocketOwner(in ParcelFileDescriptor socket, int newUid);

   /**
    * Reserve an SPI from the kernel
    *
    * @param transformId a unique identifier for allocated resources
    * @param sourceAddress InetAddress as string for the sending endpoint
    * @param destinationAddress InetAddress as string for the receiving endpoint
    * @param spi a requested 32-bit unique ID or 0 to request random allocation
    * @return the SPI that was allocated or 0 if failed
    */
    int ipSecAllocateSpi(
            int transformId,
            in @utf8InCpp String sourceAddress,
            in @utf8InCpp String destinationAddress,
            int spi);

   /**
    * Create an IpSec Security Association describing how ip(v6) traffic will be encrypted
    * or decrypted.
    *
    * @param transformId a unique identifier for allocated resources
    * @param mode either Transport or Tunnel mode
    * @param sourceAddress InetAddress as string for the sending endpoint
    * @param destinationAddress InetAddress as string for the receiving endpoint
    * @param underlyingNetId the netId of the network to which the SA is applied. Only accepted for
    *        tunnel mode SAs.
    * @param spi a 32-bit unique ID allocated to the user
    * @param markValue a 32-bit unique ID chosen by the user
    * @param markMask a 32-bit mask chosen by the user
    * @param authAlgo a string identifying the authentication algorithm to be used
    * @param authKey a byte array containing the authentication key
    * @param authTruncBits the truncation length of the MAC produced by the authentication algorithm
    * @param cryptAlgo a string identifying the encryption algorithm to be used
    * @param cryptKey a byte arrray containing the encryption key
    * @param cryptTruncBits unused parameter
    * @param aeadAlgo a string identifying the authenticated encryption algorithm to be used
    * @param aeadKey a byte arrray containing the key to be used in authenticated encryption
    * @param aeadIcvBits the truncation length of the ICV produced by the authentication algorithm
    *        (similar to authTruncBits in function)
    * @param encapType encapsulation type used (if any) for the udp encap socket
    * @param encapLocalPort the port number on the host to be used in encap packets
    * @param encapRemotePort the port number of the remote to be used for encap packets
    * @param interfaceId the identifier for the IPsec tunnel interface.
    *        Only accepted for tunnel mode SAs.
    */
    void ipSecAddSecurityAssociation(
            int transformId,
            int mode,
            in @utf8InCpp String sourceAddress,
            in @utf8InCpp String destinationAddress,
            int underlyingNetId,
            int spi,
            int markValue,
            int markMask,
            in @utf8InCpp String authAlgo, in byte[] authKey, in int authTruncBits,
            in @utf8InCpp String cryptAlgo, in byte[] cryptKey, in int cryptTruncBits,
            in @utf8InCpp String aeadAlgo, in byte[] aeadKey, in int aeadIcvBits,
            int encapType,
            int encapLocalPort,
            int encapRemotePort,
            int interfaceId);

   /**
    * Delete a previously created security association identified by the provided parameters
    *
    * @param transformId a unique identifier for allocated resources
    * @param sourceAddress InetAddress as string for the sending endpoint
    * @param destinationAddress InetAddress as string for the receiving endpoint
    * @param spi a requested 32-bit unique ID allocated to the user
    * @param markValue a 32-bit unique ID chosen by the user
    * @param markMask a 32-bit mask chosen by the user
    * @param interfaceId the identifier for the IPsec tunnel interface.
    */
    void ipSecDeleteSecurityAssociation(
            int transformId,
            in @utf8InCpp String sourceAddress,
            in @utf8InCpp String destinationAddress,
            int spi,
            int markValue,
            int markMask,
            int interfaceId);

   /**
    * Apply a previously created SA to a specified socket, starting IPsec on that socket
    *
    * @param socket a user-provided socket that will have IPsec applied
    * @param transformId a unique identifier for allocated resources
    * @param direction DIRECTION_IN or DIRECTION_OUT
    * @param sourceAddress InetAddress as string for the sending endpoint
    * @param destinationAddress InetAddress as string for the receiving endpoint
    * @param spi a 32-bit unique ID allocated to the user (socket owner)
    */
    void ipSecApplyTransportModeTransform(
            in ParcelFileDescriptor socket,
            int transformId,
            int direction,
            in @utf8InCpp String sourceAddress,
            in @utf8InCpp String destinationAddress,
            int spi);

   /**
    * Remove an IPsec SA from a given socket. This will allow unencrypted traffic to flow
    * on that socket if a transform had been previously applied.
    *
    * @param socket a user-provided socket from which to remove any IPsec configuration
    */
    void ipSecRemoveTransportModeTransform(
            in ParcelFileDescriptor socket);

   /**
    * Adds an IPsec global policy.
    *
    * @param transformId a unique identifier for allocated resources
    * @param selAddrFamily the address family identifier for the selector
    * @param direction DIRECTION_IN or DIRECTION_OUT
    * @param tmplSrcAddress InetAddress as string for the sending endpoint
    * @param tmplDstAddress InetAddress as string for the receiving endpoint
    * @param spi a 32-bit unique ID allocated to the user
    * @param markValue a 32-bit unique ID chosen by the user
    * @param markMask a 32-bit mask chosen by the user
    * @param interfaceId the identifier for the IPsec tunnel interface.
    */
    void ipSecAddSecurityPolicy(
            int transformId,
            int selAddrFamily,
            int direction,
            in @utf8InCpp String tmplSrcAddress,
            in @utf8InCpp String tmplDstAddress,
            int spi,
            int markValue,
            int markMask,
            int interfaceId);

   /**
    * Updates an IPsec global policy.
    *
    * @param transformId a unique identifier for allocated resources
    * @param selAddrFamily the address family identifier for the selector
    * @param direction DIRECTION_IN or DIRECTION_OUT
    * @param tmplSrcAddress InetAddress as string for the sending endpoint
    * @param tmplDstAddress InetAddress as string for the receiving endpoint
    * @param spi a 32-bit unique ID allocated to the user
    * @param markValue a 32-bit unique ID chosen by the user
    * @param markMask a 32-bit mask chosen by the user
    * @param interfaceId the identifier for the IPsec tunnel interface.
    */
    void ipSecUpdateSecurityPolicy(
            int transformId,
            int selAddrFamily,
            int direction,
            in @utf8InCpp String tmplSrcAddress,
            in @utf8InCpp String tmplDstAddress,
            int spi,
            int markValue,
            int markMask,
            int interfaceId);

   /**
    * Deletes an IPsec global policy.
    *
    * Deletion of global policies does not do any matching based on the templates, thus
    * template source/destination addresses are not needed (as opposed to add/update).
    *
    * @param transformId a unique identifier for allocated resources
    * @param selAddrFamily the address family identifier for the selector
    * @param direction DIRECTION_IN or DIRECTION_OUT
    * @param markValue a 32-bit unique ID chosen by the user
    * @param markMask a 32-bit mask chosen by the user
    * @param interfaceId the identifier for the IPsec tunnel interface.
    */
    void ipSecDeleteSecurityPolicy(
            int transformId,
            int selAddrFamily,
            int direction,
            int markValue,
            int markMask,
            int interfaceId);

    // This could not be declared as @uft8InCpp; thus, when used in native code it must be
    // converted from a UTF-16 string to an ASCII string.
    const String IPSEC_INTERFACE_PREFIX = "ipsec";

   /**
    * Add a IPsec Tunnel Interface.
    *
    * @param devName a unique identifier that represents the name of the device
    * @param localAddress InetAddress as string for the local endpoint
    * @param remoteAddress InetAddress as string for the remote endpoint
    * @param iKey, to match Policies and SAs for input packets.
    * @param oKey, to match Policies and SAs for output packets.
    * @param interfaceId the identifier for the IPsec tunnel interface.
    */
    void ipSecAddTunnelInterface(
            in @utf8InCpp String deviceName,
            in @utf8InCpp String localAddress,
            in @utf8InCpp String remoteAddress,
            int iKey,
            int oKey,
            int interfaceId);

   /**
    * Update a IPsec Tunnel Interface.
    *
    * @param devName a unique identifier that represents the name of the device
    * @param localAddress InetAddress as string for the local endpoint
    * @param remoteAddress InetAddress as string for the remote endpoint
    * @param iKey, to match Policies and SAs for input packets.
    * @param oKey, to match Policies and SAs for output packets.
    * @param interfaceId the identifier for the IPsec tunnel interface.
    */
    void ipSecUpdateTunnelInterface(
            in @utf8InCpp String deviceName,
            in @utf8InCpp String localAddress,
            in @utf8InCpp String remoteAddress,
            int iKey,
            int oKey,
            int interfaceId);

   /**
    * Removes a IPsec Tunnel Interface.
    *
    * @param devName a unique identifier that represents the name of the device
    */
    void ipSecRemoveTunnelInterface(in @utf8InCpp String deviceName);

   /**
    * Request notification of wakeup packets arriving on an interface. Notifications will be
    * delivered to INetdEventListener.onWakeupEvent().
    *
    * @param ifName the interface
    * @param prefix arbitrary string used to identify wakeup sources in onWakeupEvent
    */
    void wakeupAddInterface(in @utf8InCpp String ifName, in @utf8InCpp String prefix, int mark, int mask);

   /**
    * Stop notification of wakeup packets arriving on an interface.
    *
    * @param ifName the interface
    * @param prefix arbitrary string used to identify wakeup sources in onWakeupEvent
    */
    void wakeupDelInterface(in @utf8InCpp String ifName, in @utf8InCpp String prefix, int mark, int mask);

    const int IPV6_ADDR_GEN_MODE_EUI64 = 0;
    const int IPV6_ADDR_GEN_MODE_NONE = 1;
    const int IPV6_ADDR_GEN_MODE_STABLE_PRIVACY = 2;
    const int IPV6_ADDR_GEN_MODE_RANDOM = 3;

    const int IPV6_ADDR_GEN_MODE_DEFAULT = 0;
   /**
    * Set IPv6 address generation mode. IPv6 should be disabled before changing mode.
    *
    * @param mode SLAAC address generation mechanism to use
    */
    void setIPv6AddrGenMode(in @utf8InCpp String ifName, int mode);

   /**
    * Query the netd service to know if the eBPF traffic stats accounting service is currently
    * running on the device.
    */
    boolean trafficCheckBpfStatsEnable();

   /**
    * Add idletimer for specific interface
    *
    * @param ifName Name of target interface
    * @param timeout The time in seconds that will trigger idletimer
    * @param classLabel The unique identifier for this idletimer
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void idletimerAddInterface(
            in @utf8InCpp String ifName,
            int timeout,
            in @utf8InCpp String classLabel);

   /**
    * Remove idletimer for specific interface
    *
    * @param ifName Name of target interface
    * @param timeout The time in seconds that will trigger idletimer
    * @param classLabel The unique identifier for this idletimer
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void idletimerRemoveInterface(
            in @utf8InCpp String ifName,
            int timeout,
            in @utf8InCpp String classLabel);

    const int PENALTY_POLICY_ACCEPT = 1;
    const int PENALTY_POLICY_LOG = 2;
    const int PENALTY_POLICY_REJECT = 3;

   /**
    * Offers to detect sockets sending data not wrapped inside a layer of SSL/TLS encryption.
    *
    * @param uid Uid of the app
    * @param policyPenalty The penalty policy of the app
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void strictUidCleartextPenalty(int uid, int policyPenalty);

   /**
    * Start clatd
    *
    * @param ifName interface name to start clatd
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void clatdStart(in @utf8InCpp String ifName);

   /**
    * Stop clatd
    *
    * @param ifName interface name to stop clatd
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void clatdStop(in @utf8InCpp String ifName);

   /**
    * Get status of IP forwarding
    *
    * @return true if IP forwarding is enabled, false otherwise.
    */
    boolean ipfwdEnabled();

   /**
    * Enable IP forwarding for specific requester
    *
    * @param requester requester name to enable IP forwarding. It is a unique name which will be
    *                  stored in Netd to make sure if any requester needs IP forwarding.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void ipfwdEnableForwarding(in @utf8InCpp String requester);

   /**
    * Disable IP forwarding for specific requester
    *
    * @param requester requester name to disable IP forwarding. This name should match the
    *                  names which are set by ipfwdEnableForwarding.
    *                  IP forwarding would be disabled if it is the last requester.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void ipfwdDisableForwarding(in @utf8InCpp String requester);

   /**
    * Add forwarding ip rule
    *
    * @param fromIface interface name to add forwarding ip rule
    * @param toIface interface name to add forwarding ip rule
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void ipfwdAddInterfaceForward(in @utf8InCpp String fromIface, in @utf8InCpp String toIface);

   /**
    * Remove forwarding ip rule
    *
    * @param fromIface interface name to remove forwarding ip rule
    * @param toIface interface name to remove forwarding ip rule
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void ipfwdRemoveInterfaceForward(in @utf8InCpp String fromIface, in @utf8InCpp String toIface);

   /**
    * Set quota for interface
    *
    * @param ifName Name of target interface
    * @param bytes Quota value in bytes
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthSetInterfaceQuota(in @utf8InCpp String ifName, long bytes);

   /**
    * Remove quota for interface
    *
    * @param ifName Name of target interface
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthRemoveInterfaceQuota(in @utf8InCpp String ifName);

   /**
    * Set alert for interface
    *
    * @param ifName Name of target interface
    * @param bytes Alert value in bytes
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthSetInterfaceAlert(in @utf8InCpp String ifName, long bytes);

   /**
    * Remove alert for interface
    *
    * @param ifName Name of target interface
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthRemoveInterfaceAlert(in @utf8InCpp String ifName);

   /**
    * Set global alert
    *
    * @param bytes Alert value in bytes
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthSetGlobalAlert(long bytes);

   /**
    * Add naughty app bandwidth rule for specific app
    *
    * @param uid uid of target app
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthAddNaughtyApp(int uid);

   /**
    * Remove naughty app bandwidth rule for specific app
    *
    * @param uid uid of target app
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthRemoveNaughtyApp(int uid);

   /**
    * Add nice app bandwidth rule for specific app
    *
    * @param uid uid of target app
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthAddNiceApp(int uid);

   /**
    * Remove nice app bandwidth rule for specific app
    *
    * @param uid uid of target app
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void bandwidthRemoveNiceApp(int uid);

   /**
    * Start tethering
    *
    * @param dhcpRanges dhcp ranges to set.
    *                   dhcpRanges might contain many addresss {addr1, addr2, aadr3, addr4...}
    *                   Netd splits them into ranges: addr1-addr2, addr3-addr4, etc.
    *                   An odd number of addrs will fail.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void tetherStart(in @utf8InCpp String[] dhcpRanges);

   /**
    * Stop tethering
    *
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void tetherStop();

   /**
    * Get status of tethering
    *
    * @return true if tethering is enabled, false otherwise.
    */
    boolean tetherIsEnabled();

   /**
    * Setup interface for tethering
    *
    * @param ifName interface name to add
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void tetherInterfaceAdd(in @utf8InCpp String ifName);

   /**
    * Reset interface for tethering
    *
    * @param ifName interface name to remove
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void tetherInterfaceRemove(in @utf8InCpp String ifName);

   /**
    * Get the interface list which is stored in netd
    * The list contains the interfaces managed by tetherInterfaceAdd/tetherInterfaceRemove
    *
    * @return An array of strings containing interface list result
    */
    @utf8InCpp String[] tetherInterfaceList();

   /**
    * Set DNS forwarder server
    *
    * @param netId the upstream network to forward DNS queries to
    * @param dnsAddrs DNS server address to set
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void tetherDnsSet(int netId, in @utf8InCpp String[] dnsAddrs);

   /**
    * Return the DNS list set by tetherDnsSet
    *
    * @return An array of strings containing the list of DNS servers
    */
    @utf8InCpp String[] tetherDnsList();

    const int LOCAL_NET_ID = 99;

    // Route does not specify a next hop
    const String NEXTHOP_NONE = "";
    // Route next hop is unreachable
    const String NEXTHOP_UNREACHABLE = "unreachable";
    // Route next hop is throw
    const String NEXTHOP_THROW = "throw";

   /**
    * Add a route for specific network
    *
    * @param netId the network to add the route to
    * @param ifName the name of interface of the route.
    *               This interface should be assigned to the netID.
    * @param destination the destination of the route
    * @param nextHop The route's next hop address,
    *                or it could be either NEXTHOP_NONE, NEXTHOP_UNREACHABLE, NEXTHOP_THROW.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkAddRoute(
            int netId,
            in @utf8InCpp String ifName,
            in @utf8InCpp String destination,
            in @utf8InCpp String nextHop);

   /**
    * Remove a route for specific network
    *
    * @param netId the network to remove the route from
    * @param ifName the name of interface of the route.
    *               This interface should be assigned to the netID.
    * @param destination the destination of the route
    * @param nextHop The route's next hop address,
    *                or it could be either NEXTHOP_NONE, NEXTHOP_UNREACHABLE, NEXTHOP_THROW.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkRemoveRoute(
            int netId,
            in @utf8InCpp String ifName,
            in @utf8InCpp String destination,
            in @utf8InCpp String nextHop);

   /**
    * Add a route to legacy routing table for specific network
    *
    * @param netId the network to add the route to
    * @param ifName the name of interface of the route.
    *               This interface should be assigned to the netID.
    * @param destination the destination of the route
    * @param nextHop The route's next hop address,
    *                or it could be either NEXTHOP_NONE, NEXTHOP_UNREACHABLE, NEXTHOP_THROW.
    * @param uid uid of the user
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkAddLegacyRoute(
            int netId,
            in @utf8InCpp String ifName,
            in @utf8InCpp String destination,
            in @utf8InCpp String nextHop,
            int uid);

   /**
    * Remove a route from legacy routing table for specific network
    *
    * @param netId the network to remove the route from
    * @param ifName the name of interface of the route.
    *               This interface should be assigned to the netID.
    * @param destination the destination of the route
    * @param nextHop The route's next hop address,
    *                or it could be either NEXTHOP_NONE, NEXTHOP_UNREACHABLE, NEXTHOP_THROW.
    * @param uid uid of the user
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkRemoveLegacyRoute(
            int netId,
            in @utf8InCpp String ifName,
            in @utf8InCpp String destination,
            in @utf8InCpp String nextHop,
            int uid);

   /**
    * Get default network
    *
    * @return netId of default network
    */
    int networkGetDefault();

   /**
    * Set network as default network
    *
    * @param netId the network to set as the default
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkSetDefault(int netId);

   /**
    * Clear default network
    *
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkClearDefault();

    /**
     * PERMISSION_NONE is used for regular networks and apps
     */
    const int PERMISSION_NONE = 0;
    /**
     * PERMISSION_NETWORK indicates that the network is only accessible
     * to apps that have the CHANGE_NETWORK_STATE permission.
     */
    const int PERMISSION_NETWORK = 1;
    /**
     * PERMISSION_SYSTEM is used for system UIDs and apps
     * that have the CONNECTIVITY_USE_RESTRICTED_NETWORK permission
     */
    const int PERMISSION_SYSTEM = 2;

   /**
    * Sets the permission required to access a specific network.
    *
    * @param netId the network to set
    * @param permission network permission to use
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void networkSetPermissionForNetwork(int netId, int permission);

   /**
    * Assigns network access permissions to the specified users.
    *
    * @param permission network permission to use
    * @param uids uid of users to set permission
    */
    void networkSetPermissionForUser(int permission, in int[] uids);

   /**
    * Clears network access permissions for the specified users.
    *
    * @param uids uid of users to clear permission
    */
    void networkClearPermissionForUser(in int[] uids);

   /**
    * NO_PERMISSIONS indicate that the app is uninstalled and the permission need to be revoked or
    * this app doesn't have either PERMISSION_INTERNET or PERMISSION_UPDATE_DEVICE_STATS
    */
    const int NO_PERMISSIONS = 0;

   /**
    * PERMISSION_INTERNET indicates that the app can create AF_INET and AF_INET6 sockets
    */
    const int PERMISSION_INTERNET = 1;

   /**
    * PERMISSION_UPDATE_DEVICE_STATS is used for system UIDs and privileged apps
    * that have the UPDATE_DEVICE_STATS permission
    */
    const int PERMISSION_UPDATE_DEVICE_STATS = 2;

   /**
    * Assigns android.permission.INTERNET and/or android.permission.UPDATE_DEVICE_STATS to the uids
    * specified. Or remove all permissions from the uids.
    *
    * @param permission The permission to grant, it could be either PERMISSION_INTERNET and/or
    *                   PERMISSION_UPDATE_DEVICE_STATS. If the permission is NO_PERMISSIONS, then
    *                   revoke all permissions for the uids.
    * @param uids uid of users to grant permission
    */
    void trafficSetNetPermForUids(int permission, in int[] uids);

   /**
    * Gives the specified user permission to protect sockets from VPNs.
    * Typically used by VPN apps themselves, to ensure that the sockets
    * they use to communicate with the VPN server aren't routed through
    * the VPN network.
    *
    * @param uid uid of user to set
    */
    void networkSetProtectAllow(int uid);

   /**
    * Removes the permission to protect sockets from VPN.
    *
    * @param uid uid of user to set
    */
    void networkSetProtectDeny(int uid);

   /**
    * Get the status of network protect for user
    *
    * @param uids uid of user
    * @return true if the user can protect sockets from VPN, false otherwise.
    */
    boolean networkCanProtect(int uid);

    // Whitelist only allows packets from specific UID/Interface
    const int FIREWALL_WHITELIST = 0;
    // Blacklist blocks packets from specific UID/Interface
    const int FIREWALL_BLACKLIST = 1;

   /**
    * Set type of firewall
    * Type whitelist only allows packets from specific UID/Interface
    * Type blacklist blocks packets from specific UID/Interface
    *
    * @param firewalltype type of firewall, either FIREWALL_WHITELIST or FIREWALL_BLACKLIST
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void firewallSetFirewallType(int firewalltype);

    // Specify allow Rule which allows packets
    const int FIREWALL_RULE_ALLOW = 1;
    // Specify deny Rule which drops packets
    const int FIREWALL_RULE_DENY = 2;

    // No specific chain is chosen, use general firewall chain(fw_input, fw_output)
    const int FIREWALL_CHAIN_NONE = 0;
    // Specify DOZABLE chain(fw_dozable) which is used in dozable mode
    const int FIREWALL_CHAIN_DOZABLE = 1;
    // Specify STANDBY chain(fw_standby) which is used in standby mode
    const int FIREWALL_CHAIN_STANDBY = 2;
    // Specify POWERSAVE chain(fw_powersave) which is used in power save mode
    const int FIREWALL_CHAIN_POWERSAVE = 3;

   /**
    * Set firewall rule for interface
    *
    * @param ifName the interface to allow/deny
    * @param firewallRule either FIREWALL_RULE_ALLOW or FIREWALL_RULE_DENY
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void firewallSetInterfaceRule(in @utf8InCpp String ifName, int firewallRule);

   /**
    * Set firewall rule for uid
    *
    * @param childChain target chain
    * @param uid uid to allow/deny
    * @param firewallRule either FIREWALL_RULE_ALLOW or FIREWALL_RULE_DENY
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void firewallSetUidRule(int childChain, int uid, int firewallRule);

   /**
    * Enable/Disable target firewall child chain
    *
    * @param childChain target chain to enable
    * @param enable whether to enable or disable child chain.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void firewallEnableChildChain(int childChain, boolean enable);

   /**
    * Get interface list
    *
    * @return An array of strings containing all the interfaces on the system.
    * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
    *         unix errno.
    */
    @utf8InCpp String[] interfaceGetList();

    const String IF_STATE_UP = "up";
    const String IF_STATE_DOWN = "down";
    const String IF_FLAG_BROADCAST = "broadcast";
    const String IF_FLAG_LOOPBACK = "loopback";
    const String IF_FLAG_POINTOPOINT = "point-to-point";
    const String IF_FLAG_RUNNING = "running";
    const String IF_FLAG_MULTICAST = "multicast";

   /**
    * Get interface configuration
    *
    * @param ifName interface name
    * @return An InterfaceConfigurationParcel for the specified interface.
    * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
    *         unix errno.
    */
    InterfaceConfigurationParcel interfaceGetCfg(in @utf8InCpp String ifName);

   /**
    * Set interface configuration
    *
    * @param cfg Interface configuration to set
    * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
    *         unix errno.
    */
    void interfaceSetCfg(in InterfaceConfigurationParcel cfg);

   /**
    * Set interface IPv6 privacy extensions
    *
    * @param ifName interface name
    * @param enable whether to enable or disable this setting.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void interfaceSetIPv6PrivacyExtensions(in @utf8InCpp String ifName, boolean enable);

   /**
    * Clear all IP addresses on the given interface
    *
    * @param ifName interface name
    * @throws ServiceSpecificException in case of failure, with an error code corresponding to the
    *         POSIX errno.
    */
    void interfaceClearAddrs(in @utf8InCpp String ifName);

   /**
    * Enable or disable IPv6 on the given interface
    *
    * @param ifName interface name
    * @param enable whether to enable or disable this setting.
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void interfaceSetEnableIPv6(in @utf8InCpp String ifName, boolean enable);

   /**
    * Set interface MTU
    *
    * @param ifName interface name
    * @param mtu MTU value
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void interfaceSetMtu(in @utf8InCpp String ifName, int mtu);

   /**
    * Add forwarding rule/stats on given interface.
    *
    * @param intIface downstream interface
    * @param extIface upstream interface
    */
    void tetherAddForward(in @utf8InCpp String intIface, in @utf8InCpp String extIface);

   /**
    * Remove forwarding rule/stats on given interface.
    *
    * @param intIface downstream interface
    * @param extIface upstream interface
    */
    void tetherRemoveForward(in @utf8InCpp String intIface, in @utf8InCpp String extIface);

   /**
    * Set the values of tcp_{rmem,wmem}.
    *
    * @param rmemValues the target values of tcp_rmem, each value is separated by spaces
    * @param wmemValues the target values of tcp_wmem, each value is separated by spaces
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void setTcpRWmemorySize(in @utf8InCpp String rmemValues, in @utf8InCpp String wmemValues);

    /**
     * Get NAT64 prefix in format Pref64::/n which is described in RFC6147 section 2. This
     * interface is used for internal test only. Don't use it for other purposes because doing so
     * would cause race conditions with the NAT64 prefix notifications.
     *
     * @param netId the network ID of the network to get the prefix
     * @return the NAT64 prefix if the query operation was successful
     * @throws ServiceSpecificException in case of failure, with an error code indicating the
     *         cause of the the failure.
     *
     * TODO: Remove this once the tests have been updated to listen for onNat64PrefixEvent.
     */
    @utf8InCpp String getPrefix64(int netId);

   /**
    * Register unsolicited event listener
    * Netd supports multiple unsolicited event listeners, but only one per pid
    * A newer listener won't be registed if netd has an old one on the same pid.
    *
    * @param listener unsolicited event listener to register
    * @throws ServiceSpecificException in case of failure, with an error code indicating the
    *         cause of the the failure.
    */
    void registerUnsolicitedEventListener(INetdUnsolicitedEventListener listener);

}
