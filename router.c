#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "protocols.h"
#include "queue.h"
#include "lib.h"
#include "trie.h"

#define MAX_RTABLE_ENTRIES 100000
#define MAC_LEN 6
#define IP_LEN 4
#define BROADCAST_MAC 0xFFFFFFFFFFFF
#define ETHR_TYPE_IPv4 0x800
#define ETHR_TYPE_ARP 0x806
#define ICMP_PROTOCOL_NUMBER 1  // conform https://www.rfc-editor.org/rfc/rfc990 (pagina 24)
#define MAX_ARP_ENTRIES 100  // poate merge si cu mai putine din moment ce am doar 8 entitati (2 rutere + 4 host uri)
#define HARDWARE_TYPE_ETHERNET 1  // conform https://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml
#define ARP_OPERATION_REQUEST 1  // conform https://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml
#define ARP_OPERATION_REPLY 2  // conform https://www.iana.org/assignments/arp-parameters/arp-parameters.xhtml

// cand trimit pachetul mai departe, imi trebuie si lungimea lui
// creez structura packet ca sa adaug in coada nu doar continutul
// pachetelor, ci si lungimea lor
typedef struct {
	char buf[MAX_PACKET_LEN];
	int len;
} packet;

// l-as fi lasat ca macro, dar fac memcmp pe el si da segfault
const uint8_t broadcast_mac[MAC_LEN] = {255, 255, 255, 255, 255, 255};

// route table (ca in laborator)
struct route_table_entry *rtable;
int rtable_len;

// struct arp_table_entry *static_arp_table;
// int static_arp_table_len;

struct arp_table_entry arp_cache[MAX_ARP_ENTRIES];
int arp_cache_len;

char arp_pachet[sizeof(struct ether_hdr) + sizeof(struct arp_hdr)];

void send_arp(char *buf, int len, int arp_type, uint8_t src_mac[MAC_LEN], uint8_t dest_mac[MAC_LEN], 
	uint32_t src_ip, uint32_t dest_ip, int interface)
// trimit un pachet ARP de tipul `arp_type` (request sau reply)
{
	// struct ether_hdr *eth_hdr = (struct ether_hdr *)buf;
	// struct arp_hdr *arp_hdr = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));

	struct ether_hdr *eth_hdr = malloc(sizeof(struct ether_hdr));
	struct arp_hdr *arp_hdr = malloc(sizeof(struct arp_hdr));

	// populez header-ul ethernet
	eth_hdr->ethr_type = htons(ETHR_TYPE_ARP);
	memcpy(eth_hdr->ethr_dhost, dest_mac, MAC_LEN);
	memcpy(eth_hdr->ethr_shost, src_mac, MAC_LEN);

	// poplez header-ul ARP
	arp_hdr->hw_type = htons(HARDWARE_TYPE_ETHERNET);
	arp_hdr->proto_type = htons(ETHR_TYPE_IPv4);
	arp_hdr->hw_len = MAC_LEN;
	arp_hdr->proto_len = IP_LEN;
	arp_hdr->opcode = htons(arp_type);
	memcpy(arp_hdr->shwa, src_mac, MAC_LEN);
	arp_hdr->sprotoa = htonl(src_ip);
	memcpy(arp_hdr->thwa, dest_mac, MAC_LEN);
	arp_hdr->tprotoa = htonl(dest_ip);

	// construiesc pachetul ca sa pot sa il trimit
	int len_local = sizeof(struct ether_hdr) + sizeof(struct arp_hdr);
	char buf_local[len_local];

	memcpy(buf_local, eth_hdr, sizeof(struct ether_hdr));
	memcpy(buf_local + sizeof(struct ether_hdr), arp_hdr, sizeof(struct arp_hdr));

	// trimit pachetul
	send_to_link(len_local, buf_local, interface);
	printf("Trimit ARP %s: src_ip=%s, dest_ip=%s\n", arp_type == ARP_OPERATION_REQUEST ? "request" : "reply",
		 int_to_ip(src_ip), int_to_ip(ntohl(dest_ip)));
}

int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argv + 2, argc - 2);

	// aloc si citesc ca in laborator route table-ul
	rtable = malloc(sizeof(struct route_table_entry) * MAX_RTABLE_ENTRIES);
	
	if (!rtable) {
		return 1;
	}

	rtable_len = read_rtable(argv[1], rtable);

	// aloc si citesc arp table-ul static
	// static_arp_table = malloc(sizeof(struct arp_table_entry) * 10);

	// if (!static_arp_table) {
	// 	free(rtable);
	// 	return 1;
	// }

	// static_arp_table_len = parse_arp_table("arp_table.txt", static_arp_table);

	// imi creez trie-ul cu prefixe (ma pregatesc pt LPM)
	trie prefix_trie = create_trie();

	for (int i = 0; i < rtable_len; i++) {
		uint32_t prefix = ntohl(rtable[i].prefix);
		uint32_t mask = ntohl(rtable[i].mask);
		int interface = rtable[i].interface;
		uint32_t next_hop = ntohl(rtable[i].next_hop);

		printf("prefix: %s, mask: %s, interface: %d, next hop: %s\n", 
			int_to_ip(prefix), int_to_ip(mask), interface, int_to_ip(next_hop));

		prefix_trie = add_to_trie(prefix_trie, prefix, mask, interface, next_hop);

		if (!prefix_trie) {
			free(rtable);
			return 1;
		}
	}

	queue waiting_for_arp_reply_queue = create_queue();

	while (1) {
		size_t interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		// imi obtin adresele MAC si IP
		uint8_t *my_mac = (uint8_t *)malloc(MAC_LEN);
		get_interface_mac(interface, my_mac);

		struct in_addr my_ip;
		// https://man7.org/linux/man-pages/man3/inet_pton.3.html
		int result = inet_pton(AF_INET, get_interface_ip(interface), &my_ip);

		if (result <= 0) {
			free(my_mac);

			// arunc pachetul
			continue;
		}

		uint32_t my_ip_addr = ntohl(my_ip.s_addr);  // https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
		// nu puteau sa foloseasca direct unsigned long, au trebuit sa puna campul intr-o structura :(
		
		printf("my ip address: %s\n", int_to_ip(my_ip_addr));

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

		if (memcmp(dest_mac, my_mac, MAC_LEN) && memcmp(eth_hdr->ethr_dhost, 
			broadcast_mac, MAC_LEN)) {
			// n-a fost trimis nici catre mine explicit, nici catre broadcast
			free(my_mac);

			// arunc pachetul
			continue;
		}

		// printf("am verificat adresa mac de destinatie\n");

		// verific daca e de tipul IPv4
		if (eth_type == ETHR_TYPE_IPv4) {
			printf("am verificat tipul de pachet, e IPv4\n");
			struct ip_hdr *ip_hder = (struct ip_hdr *)(buf + sizeof(struct ether_hdr));

			// TODO sa vad daca intai tre sa verific checksum-ul si apoi daca e al meu pachetul
			if (ip_hder->dest_addr == my_ip_addr) {
				// pachetul e trimis catre mine
				printf("am primit pachet catre mine\n");

				// verific daca mesajul este tip ICMP
				uint8_t protocol = ip_hder->proto;

				if (protocol != ICMP_PROTOCOL_NUMBER) {
					free(my_mac);

					// routerul raspunde cf cerintei doar mesajelor ICMP, deci
					// arunc pachetul
					continue;
				}

				printf("am verificat protocolul, e ICMP\n");

				// TODO trimit ICMP echo reply
			}

			// verific checksum-ul
			uint16_t packet_checksum = ntohs(ip_hder->checksum);
			ip_hder->checksum = 0;
			uint16_t actual_checksum = checksum((uint16_t *)ip_hder, sizeof(struct ip_hdr));
			
			if (packet_checksum != actual_checksum) {
				// pachet corupt
				free(my_mac);

				// arunc pachetul
				continue;
			}

			printf("am verificat checksum-ul\n");

			// verific si actualizez TTL-ul
			if (!ip_hder->ttl || ip_hder->ttl == 1) {
				// TTL-ul a expirat
				// TODO trimit ICMP "Time Exceeded" catre sursa

				free(my_mac);

				// arunc pachetul
				continue;
			}

			ip_hder->ttl--;

			printf("am verificat si modificat TTL-ul\n");

			// LPM, caut interfata si adresa urmatorului hop
			uint32_t ip_addr_dest = ntohl(ip_hder->dest_addr);
			LPM lpm = longest_prefix_match(prefix_trie, ip_addr_dest);

			uint32_t next_hop_addr = lpm.ip_addr;
			int next_hop_interface = lpm.interface;

			printf("am gasit interfata %d si adresa %s pt next hop\n", 
				next_hop_interface, int_to_ip(next_hop_addr));

			if (next_hop_interface == -1) {
				// nu am gasit un prefix care sa se potriveasca
				// TODO trimit ICMP "Destination Unreachable" catre sursa
				
				free(my_mac);

				// arunc pachetul
				continue;
			}

			uint32_t next_hop_addr_network = htonl(next_hop_addr);
			ip_hder->dest_addr = next_hop_addr_network;
			ip_hder->source_addr = my_ip_addr;

			// actualizez checksum-ul
			uint16_t new_checksum = checksum((uint16_t *)ip_hder, sizeof(struct ip_hdr));
			ip_hder->checksum = htons(new_checksum);

			printf("am modificat checksum-ul\n");

			// determin adresa mac a urmatorului hop
			uint8_t next_hop_mac[MAC_LEN];
			memcpy(next_hop_mac, broadcast_mac, MAC_LEN);

			printf("adresa IP a urmatorului hop host: %s, network: %s\n", 
				int_to_ip(next_hop_addr), int_to_ip(next_hop_addr_network));

			// caut in cache daca exista deja un entry pt adresa ip curenta
			for (int i = 0; i < arp_cache_len; i++) {
				if (arp_cache[i].ip == next_hop_addr_network) {
					memcpy(next_hop_mac, arp_cache[i].mac, MAC_LEN);
				}
			}

			if (!memcmp(next_hop_mac, broadcast_mac, MAC_LEN)) {
				// nu am gasit adresa ip in cache-ul ARP
				// adaug pachetul in coada ca sa il prelucrez dupa ce primesc ARP reply
				packet ipv4_packet;
				memcpy(ipv4_packet.buf, buf, len);
				ipv4_packet.len = len;
				
				queue_enq(waiting_for_arp_reply_queue, &ipv4_packet);
				
				// trebuie sa trimit un ARP request catre broadcast
				send_arp(buf, len, ARP_OPERATION_REQUEST, my_mac, (uint8_t *)broadcast_mac, my_ip_addr, next_hop_addr, next_hop_interface);
				
				printf("am trimis ARP request catre broadcast\n");
				
				free(my_mac);
				continue;
			}

			printf("am gasit adresa MAC a urmatorului hop\n");

			// trimit pachet catre urmatorul hop
			memcpy(eth_hdr->ethr_dhost, next_hop_mac, MAC_LEN);
			memcpy(eth_hdr->ethr_shost, my_mac, MAC_LEN);

			printf("am modificat adresele MAC sursa si destinatie\n");

			send_to_link(len, buf, next_hop_interface);

			printf("am trimis pachetul catre urmatorul hop\n");
		} else if (eth_type == ETHR_TYPE_ARP) {
			printf("ARP\n");

			struct ether_hdr *eth_hdr = (struct ether_hdr *)buf;
			struct arp_hdr *arp_hdr = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));

			// verific daca e ARP request sau reply
			uint16_t opcode = ntohs(arp_hdr->opcode);

			if (opcode != ARP_OPERATION_REQUEST && opcode != ARP_OPERATION_REPLY) {
				// accept doar operatii de tip request sau reply
				free(my_mac);

				printf("am aruncat pachetul, nu e ARP request sau reply\n");
				// arunc pachetul
				continue;
			}

			printf("am verificat tipul de pachet, request sau reply\n");

			// verific daca e ARP request
			if (opcode == ARP_OPERATION_REQUEST) {
				printf("ARP request\n");

				// verific daca e ARP request catre mine
				if (ntohl(arp_hdr->tprotoa) == my_ip_addr) {
					// trimit ARP reply cu mac-ul meu
					uint8_t dest_mac[MAC_LEN];
					memcpy(dest_mac, eth_hdr->ethr_shost, MAC_LEN);
					uint32_t dest_ip = arp_hdr->sprotoa;

					printf("am primit ARP request catre mine, trimit ARP reply\n");
					printf("Am primit ARP Request: src_ip=%s, src_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
						int_to_ip(dest_ip),
						arp_hdr->shwa[0], arp_hdr->shwa[1], arp_hdr->shwa[2],
						arp_hdr->shwa[3], arp_hdr->shwa[4], arp_hdr->shwa[5]);
					
					send_arp(buf, len, ARP_OPERATION_REPLY, my_mac, dest_mac, my_ip_addr, ntohl(dest_ip), interface);
					continue;
				}

				// nu e ARP request catre mine
				// il dau mai departe
				struct ether_hdr *eth_hdr = (struct ether_hdr *)buf;
				struct arp_hdr *arp_hdr = (struct arp_hdr *)(buf + sizeof(struct ether_hdr));

				memcpy(eth_hdr->ethr_shost, my_mac, MAC_LEN);
				memcpy(eth_hdr->ethr_dhost, broadcast_mac, MAC_LEN);
				memcpy(arp_hdr->thwa, broadcast_mac, MAC_LEN);

				// memcpy(arp_hdr->shwa, my_mac, MAC_LEN);
				// arp_hdr->sprotoa = my_ip_addr;

				send_to_link(len, buf, interface);

				printf("am trimis ARP request mai departe\n");

				free(my_mac);

				continue;
			}

			printf("ARP reply\n");
			printf("Am primit ARP Reply: src_ip=%s, src_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
				int_to_ip(arp_hdr->sprotoa),
				arp_hdr->shwa[0], arp_hdr->shwa[1], arp_hdr->shwa[2],
				arp_hdr->shwa[3], arp_hdr->shwa[4], arp_hdr->shwa[5]);

			// adaug in cache adresele IP si MAC
			// presupun ca nu pot primi ARP reply daca am deja in cache adresa MAC
			memcpy(arp_cache[arp_cache_len].mac, arp_hdr->shwa, MAC_LEN);
			arp_cache[arp_cache_len].ip = arp_hdr->sprotoa;
			arp_cache_len++;

			printf("am adaugat in cache adresa IP si MAC\n");

			// caut in coada pachetul care asteapta ARP reply
			// atentie ca trebuie sa se potriveasca IP-ul sursa din ARP reply
			// cu IP-ul destinatie din pachetul care asteapta ARP reply

			// trebuie sa pastrez pachetele care nu se potrivesc, altfel le pierd
			queue not_the_packet_i_wanted = create_queue();

			while (!queue_empty(waiting_for_arp_reply_queue)) {
				packet *ipv4_packet = (packet *)queue_deq(waiting_for_arp_reply_queue);
				struct ip_hdr *ip_hder = (struct ip_hdr *)(ipv4_packet->buf + sizeof(struct ether_hdr));

				printf("am scos un pachet din coada, ip dest %s, arp ip %s\n", 
					int_to_ip(ip_hder->dest_addr), int_to_ip(arp_hdr->sprotoa));

				if (ip_hder->dest_addr == arp_hdr->sprotoa) {
					// am gasit pachetul care astepta ARP reply
					printf("am gasit pachetul care astepta ARP reply\n");
					memcpy(eth_hdr->ethr_dhost, arp_hdr->shwa, MAC_LEN);
					memcpy(eth_hdr->ethr_shost, my_mac, MAC_LEN);

					send_to_link(ipv4_packet->len, ipv4_packet->buf, interface);
				} else {
					queue_enq(not_the_packet_i_wanted, ipv4_packet);
				}
			}

			// pun inapoi pachetele care nu s-au potrivit
			while (!queue_empty(not_the_packet_i_wanted)) {
				packet *ipv4_packet = (packet *)queue_deq(not_the_packet_i_wanted);
				queue_enq(waiting_for_arp_reply_queue, ipv4_packet);
			}
		}
		
		free(my_mac);

    /* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */
	}

	free(rtable);
	// free(static_arp_table);
	free_trie(&prefix_trie);
}

