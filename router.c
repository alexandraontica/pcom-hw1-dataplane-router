#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "trie.h"

#define MAX_RTABLE_ENTRIES 65000
#define MAC_LEN 6
#define BROADCAST_MAC 0xFFFFFFFFFFFF
#define ETHR_TYPE_IPv4 0x800
#define ETHR_TYPE_ARP 0x806
#define ICMP_PROTOCOL_NUMBER 1  // conform https://www.rfc-editor.org/rfc/rfc990 (pagina 24)

// l-as fi lasat ca macro, dar fac memcmp pe el si da segfault
const uint8_t broadcast_mac[MAC_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// route table (ca in laborator)
struct route_table_entry *rtable;
int rtable_len;

struct arp_table_entry *static_arp_table;
int static_arp_table_len;

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	// aloc ca in laborator route table-ul si il citesc
	rtable = malloc(sizeof(struct route_table_entry) * MAX_RTABLE_ENTRIES);
	
	if (!rtable) {
		return 1;
	}

	rtable_len = read_rtable(argv[1], rtable);

	// aloc si citesc arp table-ul static
	static_arp_table = malloc(sizeof(struct arp_table_entry) * 10);

	if (!static_arp_table) {
		free(rtable);
		return 1;
	}

	static_arp_table_len = parse_arp_table("arp_table.txt", static_arp_table);

	// imi creez trie-ul cu prefixe (ma pregatesc pt LPM)
	trie prefix_trie = create_trie();

	for (int i = 0; i < rtable_len; i++) {
		uint32_t prefix = rtable[i].prefix;
		uint32_t mask = rtable[i].mask;
		int interface = rtable[i].interface;
		uint32_t next_hop = rtable[i].next_hop;

		prefix_trie = add_to_trie(prefix_trie, prefix, mask, interface, next_hop);

		if (!prefix_trie) {
			free(rtable);
			return 1;
		}
	}

	while (1) {

		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		uint8_t *my_mac = (uint8_t *)malloc(MAC_LEN);
		get_interface_mac(interface, my_mac);

		struct ether_hdr *eth_hdr = (struct ether_hdr *)buf;

		// verific daca e de tip ethernet
		uint16_t eth_type = ntohs(eth_hdr->ethr_type);
		if (eth_type != ETHR_TYPE_IPv4 && eth_type != ETHR_TYPE_ARP) {
			free(my_mac);

			// arunc pachetul
			continue;
		}

		// verific daca mi-a fost trimis mie pachetul (sau catre broadcast)

		// TODO sa verific daca merge asa sau daca trebuie sa fac memcpy
		// uint8_t dest_mac[MAC_LEN] = eth_hdr->ethr_dhost;
		uint8_t dest_mac[MAC_LEN];
		memcpy(dest_mac, eth_hdr->ethr_dhost, MAC_LEN);

		if (memcmp(dest_mac, my_mac, MAC_LEN) != 0 && memcmp(eth_hdr->ethr_dhost, broadcast_mac, MAC_LEN) != 0) {
			// n-a fost trimis nici catre mine explicit, nici catre broadcast
			free(my_mac);

			// arunc pachetul
			continue;
		}

		// verific daca e de tipul IPv4
		if (eth_type == ETHR_TYPE_IPv4) {
			struct ip_hdr *ip_hder = (struct ip_hdr *)(buf + sizeof(struct ether_hdr));

			// verific daca mesajul este tip ICMP
			uint8_t protocol = ip_hder->proto;

			if (protocol != ICMP_PROTOCOL_NUMBER) {
				free(my_mac);

				// routerul raspunde cf cerintei doar mesajelor ICMP, deci
				// arunc pachetul
				continue;
			}

			// verific checksum-ul
			uint16_t package_checksum = ntohs(ip_hder->checksum);
			ip_hder->checksum = 0;
			uint16_t actual_checksum = checksum((uint16_t *)ip_hder, sizeof(struct ip_hdr));
			
			if (package_checksum != actual_checksum) {
				// pachet corupt
				free(my_mac);

				// arunc pachetul
				continue;
			}

			// verific si actualizez TTL-ul
			if (ip_hder->ttl == 0 || ip_hder->ttl == 1) {
				// TTL-ul a expirat
				// TODO trimite ICMP "Time Exceeded" catre sursa

				free(my_mac);

				// arunc pachetul
				continue;
			}

			// LPM, caut interfata si adresa urmatorului hop
			// TODO sa verific daca merge asa sau daca trebuie ntohl
			uint32_t ip_addr_dest = ntohl(ip_hder->dest_addr);
			LPM lpm = longest_prefix_match(prefix_trie, ip_addr_dest);

			uint32_t next_hop_addr = lpm.ip_addr;
			int next_hop_interface = lpm.interface;

			if (next_hop_interface == -1) {
				// nu am gasit un prefix care sa se potriveasca
				// TODO trimite ICMP "Destination Unreachable" catre sursa
				
				free(my_mac);

				// arunc pachetul
				continue;
			}

			// actualizez checksum-ul
			uint16_t new_checksum = checksum((uint16_t *)ip_hder, sizeof(struct ip_hdr));
			ip_hder->checksum = htons(new_checksum);

			// TODO determin adresa mac a urmatorului hop (ARP)
			uint32_t next_hop_addre_network = htonl(next_hop_addr);
			uint8_t next_hop_mac[MAC_LEN];

			for (int i = 0; i < static_arp_table_len; i++) {
				if (static_arp_table[i].ip == next_hop_addre_network) {
					memcpy(next_hop_mac, static_arp_table[i].mac, MAC_LEN);
					break;
				}
			}

			// TODO trimit pachet catre urmatorul hop
			memcpy(eth_hdr->ethr_dhost, next_hop_mac, MAC_LEN);
			memcpy(eth_hdr->ethr_shost, my_mac, MAC_LEN);

			send_to_link(len, buf, next_hop_interface);

			free(my_mac);
		}

		// TODO daca nu e ipv4, verific daca e arp
		

    /* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */
	}

	free(rtable);
	free(static_arp_table);
	free_trie(&prefix_trie);
}

