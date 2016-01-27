#include "ip_out.h"
#include "ip_in.h"
#include "eth_out.h"
#include "arp.h"
#include "debug.h"
#include "config.h"

/*----------------------------------------------------------------------------*/
inline int
GetOutputInterface(uint32_t daddr)
{
	int nif = -1;
	int i;
	int prefix = 0;

	/* Longest prefix matching */
	unsigned char *helper = (unsigned char *)&daddr;
	TRACE_CONFIG("OUR DADDR: %i.%i.%i.%i\n",helper[0],helper[1],helper[2],helper[3]);
	for (i = 0; i < CONFIG.routes; i++) {
		TRACE_CONFIG("----ROUTE %i----\n", i);
		//daddr
		helper = (unsigned char *)&CONFIG.rtable[i].daddr;
		TRACE_CONFIG("DADDR: %i.%i.%i.%i\n",helper[0],helper[1],helper[2],helper[3]);
		//mask
		helper = (unsigned char *)&CONFIG.rtable[i].mask;
		TRACE_CONFIG("MASK: %i.%i.%i.%i\n",helper[0],helper[1],helper[2],helper[3]);
		//masked
		helper = (unsigned char *)&CONFIG.rtable[i].masked;
		TRACE_CONFIG("MASKED: %i.%i.%i.%i\n",helper[0],helper[1],helper[2],helper[3]);
		//prefix & nif
		TRACE_CONFIG("PREFIX: %i\nNIF: %i\n", CONFIG.rtable[i].prefix, CONFIG.rtable[i].nif);
		if ((daddr & CONFIG.rtable[i].mask) == CONFIG.rtable[i].masked) {
			if (CONFIG.rtable[i].prefix > prefix) {
				nif = CONFIG.rtable[i].nif;
				prefix = CONFIG.rtable[i].prefix;
			}
		}
	}

	if (nif < 0) {
		uint8_t *da = (uint8_t *)&daddr;
		TRACE_ERROR("[WARNING] No route to %u.%u.%u.%u\n", 
				da[0], da[1], da[2], da[3]);
		assert(0);
	}
	
	return nif;
}
/*----------------------------------------------------------------------------*/
uint8_t *
IPOutputStandalone(struct mtcp_manager *mtcp, 
		uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t tcplen)
{
	struct iphdr *iph;
	int nif;
	unsigned char * haddr;

	nif = GetOutputInterface(daddr);
	if (nif < 0)
		return NULL;

	haddr = GetDestinationHWaddr(daddr);
	if (!haddr) {
#if 0
		uint8_t *da = (uint8_t *)&daddr;
		TRACE_INFO("[WARNING] The destination IP %u.%u.%u.%u "
				"is not in ARP table!\n",
				da[0], da[1], da[2], da[3]);
#endif
		RequestARP(mtcp, daddr, nif, mtcp->cur_ts);
		return NULL;
	}
	TRACE_CONFIG("IPOutputStandalone called with tcplen = %i\n", tcplen);

	iph = (struct iphdr *)EthernetOutput(mtcp, 
			ETH_P_IP, nif, haddr, tcplen + IP_HEADER_LEN);
	if (!iph) {
		return NULL;
	}

	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + tcplen);
	iph->id = htons(ip_id);
	iph->frag_off = htons(0x4000);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = saddr;
	iph->daddr = daddr;
	iph->check = 0;
	iph->check = ip_fast_csum(iph, iph->ihl);

	return (uint8_t *)(iph + 1);
}
/*----------------------------------------------------------------------------*/
uint8_t *
IPOutput(struct mtcp_manager *mtcp, tcp_stream *stream, uint16_t tcplen)
{
	struct iphdr *iph;
	int nif;
	unsigned char *haddr;

	if (stream->sndvar->nif_out >= 0) {
		nif = stream->sndvar->nif_out;
	} else {
		nif = GetOutputInterface(stream->daddr);
		stream->sndvar->nif_out = nif;
	}

	haddr = GetDestinationHWaddr(stream->daddr);
	if (!haddr) {
#if 0
		uint8_t *da = (uint8_t *)&stream->daddr;
		TRACE_INFO("[WARNING] The destination IP %u.%u.%u.%u "
				"is not in ARP table!\n",
				da[0], da[1], da[2], da[3]);
#endif
		/* if not found in the arp table, send arp request and return NULL */
		/* tcp will retry sending the packet later */
		RequestARP(mtcp, stream->daddr, stream->sndvar->nif_out, mtcp->cur_ts);
		return NULL;
	}
	uint8_t *da = (uint8_t *)&stream->daddr;
	uint8_t *sa = (uint8_t *)&stream->saddr;
	TRACE_CONFIG("IPOutput called with tcplen=%hi\n", tcplen);
	TRACE_CONFIG("    Source: ipaddr=%u.%u.%u.%u, nif_out=%u\n", sa[0], sa[1], sa[2], sa[3], stream->sndvar->nif_out);
	TRACE_CONFIG("    Dest: ipaddr=%u.%u.%u.%u, hwaddr=%hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n", da[0], da[1], da[2], da[3], haddr[0], haddr[1], haddr[2], haddr[3], haddr[4], haddr[5]);
	iph = (struct iphdr *)EthernetOutput(mtcp, ETH_P_IP, 
			stream->sndvar->nif_out, haddr, tcplen + IP_HEADER_LEN);
	if (!iph) {
		return NULL;
	}

	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + tcplen);
	iph->id = htons(stream->sndvar->ip_id++);
	iph->frag_off = htons(0x4000);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = stream->saddr;
	iph->daddr = stream->daddr;
	iph->check = 0;
	iph->check = ip_fast_csum(iph, iph->ihl);

	return (uint8_t *)(iph + 1);
}
