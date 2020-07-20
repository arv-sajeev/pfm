# GTPv1-U GPRS Tunneling protocol Userplane 

GTP is a group of IP based protocols that are used within the core network since GSM, UMTS and now 5G-NR.
In 5G-NR core network it is used for tunneling between the CUUP and DU (F1-U interface) and the CUUP - UPF 
interface (N3 interface). Our implementation uses 3GPP TS 29.281 V15.7.0  specification as reference.

## Definitions

### Common tunnel endpoint identifier (C-TEID)
* it unambiguously identifies a tunnel entity in the receiving side for a given UDP/IP endpoint.
* Tunnels are assigned locally and are signalled to the destination via control plane signalling.
* 
