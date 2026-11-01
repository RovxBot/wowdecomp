#ifndef WOWDECOMP_NATIVE_SESSION_H
#define WOWDECOMP_NATIVE_SESSION_H
class NetClient;
namespace NativeSession {
// Called after WowConnectionNet::Poll on the same Event owner. No worker callback.
void CheckOwner();
void Poll();
void Attach(NetClient *client);
void Detach(NetClient *client);
} // namespace NativeSession
#endif
