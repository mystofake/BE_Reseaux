#include <mictcp.h>
#include <api/mictcp_core.h>

#define __TCPMIC_BUFSIZE 32

int __TCP_MIC_current = 0;

mic_tcp_sock __MIC_TCP_sock_buffer[__TCPMIC_BUFSIZE];

int mic_tcp_first_available_fd(void)
{
	int i, ret = 0;
	for(i=0;i<__TCPMIC_BUFSIZE;++i)
	{
		if(__MIC_TCP_sock_buffer[i].fd == ret)
		{
			i = -1;
			++ret;
		}
	}
	return ret;
}

/*
mic_tcp_get_id_from_fd
retour : -1 si erreur, id dans __MIC_TCP_sock_buffer sinon
*/
int mic_tcp_get_id_from_fd(int fd)
{
	int i;
	for(i=0;i<__TCPMIC_BUFSIZE;++i)
	{
		if(__MIC_TCP_sock_buffer[i].fd == fd)
		{
			return i;
		}
	}
	return -1;
}

#define TCPMIC_SYN 1
#define TCPMIC_ACK 2
#define TCPMIC_FIN 4

mic_tcp_header mic_tcp_build_header(int flag, unsigned int ack_num,
					unsigned int seq_num, unsigned short source_port, unsigned short dest_port)
{
	mic_tcp_header header;
	
	header.syn = (((flag&TCPMIC_SYN)!=0)?(1):(0));
	header.ack = (((flag&TCPMIC_ACK)!=0)?(1):(0));
	header.fin = (((flag&TCPMIC_FIN)!=0)?(1):(0));

	header.ack_num = ack_num;
	header.seq_num = seq_num;
	header.source_port = source_port;
	header.dest_port = dest_port;

	return header;
}

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm) 
{/*
   int result = -1;
   
	
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm);  Appel obligatoire 
   set_loss_rate(0);

   return result;*/

	printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

	if(initialize_components(sm) == -1)
	{
		return -1;
	}

	set_loss_rate(0);

	if(__TCP_MIC_current >= __TCPMIC_BUFSIZE)
	{
		return -1;
	}

	__MIC_TCP_sock_buffer[__TCP_MIC_current].state = sm;
	__MIC_TCP_sock_buffer[__TCP_MIC_current].fd =  mic_tcp_first_available_fd();

	return __MIC_TCP_sock_buffer[__TCP_MIC_current].fd;
}

/*
 * Permet d’attribuer une adresse à un socket. 
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
	printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

	int id = mic_tcp_get_id_from_fd(socket);

	if(id == -1)
	{
		return -1;
	}

	__MIC_TCP_sock_buffer[id].addr = addr;


	return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
	printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");

	int id = mic_tcp_get_id_from_fd(socket);

	if(id == -1)
	{
		return -1;
	}

	/*if(__MIC_TCP_sock_buffer[id].state != IDLE)
	{
		return -1;
	}
	__MIC_TCP_sock_buffer[id].state = WAIT_CONNEXION;*/
	
	//completer
	
	return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

	int id = mic_tcp_get_id_from_fd(mic_sock);

	if(id == -1)
	{
		return -1;
	}

	mic_tcp_pdu pdu;

	pdu.header = mic_tcp_build_header(0, 0/* inutile a priori */, __MIC_TCP_sock_buffer[id].n_seq, 1512, __MIC_TCP_sock_buffer[id].addr.port);
	pdu.payload.data = mesg;
	pdu.payload.size = mesg_size;

	return IP_send(pdu, __MIC_TCP_sock_buffer[id].addr);
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{

	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");

	mic_tcp_payload content;

	content.data = malloc(sizeof(char)*max_mesg_size);
	content.size = app_buffer_get(content);
	
	if(content.size == -1)
	{
		return -1;
	}

	if(content.size >= max_mesg_size)
	{
		content.size = max_mesg_size;
	}
    
	strncpy(mesg, content.data, content.size);

	return content.size;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    return -1;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
	printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
	
	mic_tcp_pdu received_pdu;
	mic_tcp_sock_addr addr_emetteur;
	unsigned long timeout = 500; // En ms
	int pdu_size;

	/*while(pdu_size <= 0)
	{
		pdu_size = IP_recv(&received_pdu,&addr_emetteur, timeout);printf("0\n");
	}*/

	if(pdu.payload.data > 0)
	{
		app_buffer_put(pdu.payload);
	}
}

