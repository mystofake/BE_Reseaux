#include <mictcp.h>
#include <api/mictcp_core.h>

#define __TCPMIC_BUFSIZE 32
#define __TCPMIC_MAXWAITLOOPSIZE 1024

int __TCP_MIC_current = 0; // actual buffer size

mic_tcp_sock __MIC_TCP_sock_buffer[__TCPMIC_BUFSIZE];

// Retourne le premier descripteur disponible
int mic_tcp_first_available_fd(void)
{
	int i, ret = 0;
	for(i=0;i<__TCP_MIC_current;++i)
	{
		if(__MIC_TCP_sock_buffer[i].fd == ret)
		{
			i = -1;
			++ret;
		}
	}
	return ret;
}

int mic_tcp_first_available_for_ack_from_addr(mic_tcp_sock_addr addr)
{ // get the first socket that is waiting for an ack on addr
	int i;
	for(i=0;i<__TCP_MIC_current;++i)
	{
		//printf("%.*s :::: %.*s\n", sizeof(mic_tcp_sock_addr), &__MIC_TCP_sock_buffer[i].addr, sizeof(mic_tcp_sock_addr),  &addr);
		//if((memcmp(&__MIC_TCP_sock_buffer[i].addr, &addr, sizeof(mic_tcp_sock_addr))) && (__MIC_TCP_sock_buffer[i].state == WAIT_FOR_ACK))
		if((__MIC_TCP_sock_buffer[i].addr.port == addr.port) && (__MIC_TCP_sock_buffer[i].state == CONNECTED))
		{
			return i;
		}
		else
		{
			printf("%d != %d\n", __MIC_TCP_sock_buffer[i].addr.port, addr.port);
			printf("or %d pas bon etat\n\n", __MIC_TCP_sock_buffer[i].state);
		}
	}
	return -1;
}

/*
mic_tcp_get_id_from_fd
retour : -1 si erreur, id dans __MIC_TCP_sock_buffer sinon
*/
int mic_tcp_get_id_from_fd(int fd)
{
	int i;
	for(i=0;i<__TCP_MIC_current;++i)
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

	__MIC_TCP_sock_buffer[__TCP_MIC_current].state = IDLE;
	__MIC_TCP_sock_buffer[__TCP_MIC_current].fd =  mic_tcp_first_available_fd();
	__MIC_TCP_sock_buffer[__TCP_MIC_current].n_seq =  0;


	++__TCP_MIC_current;
	return __MIC_TCP_sock_buffer[__TCP_MIC_current-1].fd;
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
	
	if(__MIC_TCP_sock_buffer[id].state != IDLE)
	{
		return -1;
	}

	__MIC_TCP_sock_buffer[id].state = BOUND;
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

	if(__MIC_TCP_sock_buffer[id].state != BOUND)
	{
		return -1;
	}

	__MIC_TCP_sock_buffer[id].state = WAIT_CONNEXION;
	
	//completer
	__MIC_TCP_sock_buffer[id].state = CONNECTED;
	
	return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    
    int id = mic_tcp_get_id_from_fd(socket);

	if(id == -1)
	{
		return -1;
	}

	if(__MIC_TCP_sock_buffer[id].state != IDLE)
	{
		return -1;
	}

	__MIC_TCP_sock_buffer[id].state = CONNECTED;

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
		printf("erreur socket not found\n");
		return -1;
	}

	int i;
	for(i=0;i<__TCPMIC_MAXWAITLOOPSIZE;++i)
	{//printf("current : %d\n", __MIC_TCP_sock_buffer[id].state);
		if(__MIC_TCP_sock_buffer[id].state == CONNECTED)
		{ // waiting the socket to be available
			break;
		}
		usleep(500); // wait 500 usec (arbitrary value)
	}
	if(i==__TCPMIC_MAXWAITLOOPSIZE)
	{ // if the socket doesn't come to the connected state in less than MAXWAITLOOPSIZE iterations : error
		return -1;
	}
	
	printf("proceeding with state %d\n", __MIC_TCP_sock_buffer[id].state);


	
	// Création du PDU
	mic_tcp_pdu pdu;
	pdu.header = mic_tcp_build_header(0, 0/* inutile a priori */, __MIC_TCP_sock_buffer[id].n_seq, __MIC_TCP_sock_buffer[id].addr.port, __MIC_TCP_sock_buffer[id].addr.port);
	pdu.payload.data = mesg;
	pdu.payload.size = mesg_size;

	int ret;
	mic_tcp_pdu pduAck;
	pduAck.payload.size = 0;
	mic_tcp_sock_addr addr;


	do
	{
		printf("envoi du message avec numéro de séquence : %d \n", __MIC_TCP_sock_buffer[id].n_seq);
		ret = IP_send(pdu, __MIC_TCP_sock_buffer[id].addr);
		if(ret == -1)
		{
			printf("unable to send message");
			exit(1);
		}
		
		// Le message est envoyé --> On attend acquittement
		__MIC_TCP_sock_buffer[id].state = WAIT_FOR_ACK;


		// On utilise le timer de IP_recv
		ret = IP_recv(&pduAck, &addr, 2000);
		if(ret != -1)
		{
			if(pduAck.header.ack && (pduAck.header.ack_num == __MIC_TCP_sock_buffer[id].n_seq))
			{	
				printf("acquittement recu !\n");
				break;
			}
			else
			{
				printf("pb dans l'acquittement\n");
			}
		}
		else
		{
			printf("rien recu : %d\n", ret);
		}


	}while(__MIC_TCP_sock_buffer[id].state == WAIT_FOR_ACK);
	
	__MIC_TCP_sock_buffer[id].state = CONNECTED;
	if(__MIC_TCP_sock_buffer[id].n_seq == 0)
	{
		__MIC_TCP_sock_buffer[id].n_seq = 1;
	}
	else
	{
		__MIC_TCP_sock_buffer[id].n_seq = 0;
	}

	return ret;
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
	
	int id = mic_tcp_get_id_from_fd(socket);

	if(id == -1)
	{
		return -1;
	}

	mic_tcp_payload content;
	switch(__MIC_TCP_sock_buffer[id].state)
	{
		case CONNECTED:
			content.data = malloc(sizeof(char)*max_mesg_size);
			content.size = max_mesg_size;
			content.size = app_buffer_get(content);
	
			if(content.size == -1)
			{ // if there was an error retrieving from buffer
				printf("error retrieving from buffer");
				free(content.data);
				return -1;
			}
		
			strncpy(mesg, content.data, content.size);
			free(content.data);

			return content.size;
			break;

		case WAIT_FOR_ACK:
			printf("waiting for ack !\n");
			return 0;
			
			break;

		default:
			return -1; // not a valid state
			break;
	}
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
	

	int id_socket = mic_tcp_first_available_for_ack_from_addr(addr);

	if(id_socket == -1)
	{
		printf("personne n'ecoute\n");
		// no socket can get the PDU
		//return;
		id_socket = 0;
	}

	if(pdu.header.ack != 1) // if it is not a ack pdu
	{
		mic_tcp_header ack_header = mic_tcp_build_header(TCPMIC_ACK, pdu.header.seq_num, 0/*unused*/, __MIC_TCP_sock_buffer[id_socket].addr.port, __MIC_TCP_sock_buffer[id_socket].addr.port);
		mic_tcp_pdu ack_pdu;
		ack_pdu.header = ack_header;
		ack_pdu.payload.data = NULL;
		ack_pdu.payload.size = 0;

		// On envoie l'acquittement du PDU reçu avec N°ACK = N°SEQ_reçu
		printf("envoi de l'acquittement, on acquitte %d \n", __MIC_TCP_sock_buffer[id_socket].n_seq);
		IP_send(ack_pdu, addr);


		if(pdu.payload.data > 0) 
		{
			// On délivre les datas si les numéros coincident + on met a jour numéro seq
			if(pdu.header.seq_num == __MIC_TCP_sock_buffer[id_socket].n_seq)
			{
				app_buffer_put(pdu.payload);
				if(__MIC_TCP_sock_buffer[id_socket].n_seq == 0)
				{
					__MIC_TCP_sock_buffer[id_socket].n_seq = 1;
				}
				else
				{
					__MIC_TCP_sock_buffer[id_socket].n_seq = 0;
				}
			}
		}
	}
}

