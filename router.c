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

struct mac_entry *mac_table;
int mac_table_len;

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	// aloc ca in laborator route table-ul si il citesc
	rtable = malloc(sizeof(struct route_table_entry) * MAX_RTABLE_ENTRIES);
	DIE(rtable == NULL, "malloc rtable");

	rtable_len = read_rtable(argv[1], rtable);
	DIE(rtable_len < 0, "read rtable");

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

		// TODO verifica daca merge asa sau daca trebuie sa fac memcpy
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

			// TODO verific daca eu sunt destinatia????

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

			// TODO verific si actualizez TTL-ul
			if (ip_hder->ttl == 0 || ip_hder->ttl == 1) {
				// TTL-ul a expirat
				// TODO trimite ICMP "Time Exceeded" catre sursa

				free(my_mac);

				// arunc pachetul
				continue;
			}

			// TODO caut in tabel urmatorul hop + LPM

			// TODO actualizez checksum-ul

			// TODO actualizez adresa sursa

			// TODO determin adresa urmatorului hop (ARP)

			// TODO trimit pachet catre urmatorul hop
		}

		// TODO daca nu e ipv4, verific daca e arp
		

    /* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */
	}
}

