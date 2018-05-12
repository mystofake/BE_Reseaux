#include <mictcp.h>
#include <api/mictcp_core.h>


#define __TCPMIC_BUFSIZE 32
#define __TCPMIC_MAXWAITLOOPSIZE 1024

int __TCP_MIC_current = 0; // actual buffer size

mic_tcp_sock __MIC_TCP_sock_buffer[__TCPMIC_BUFSIZE];


// Affichage d'un PDU
void print_PDU(mic_tcp_pdu PDU)
{
	printf("*****************PDU*****************\n");
	printf("SYN : %d - ACK : %d - FIN : %d \n", PDU.header.syn, PDU.header.ack, PDU.header.fin);
	printf("Port source : %d - Port dest : %d \n", PDU.header.source_port, PDU.header.dest_port);
	printf("SEQ num : %d - ACK num : %d\n", PDU.header.seq_num, PDU.header.ack_num);
	if(PDU.payload.size != 0)
		printf("Contenu : %.*s - Taille : %d \n", PDU.payload.size, PDU.payload.data, PDU.payload.size);
	printf("**********************************\n\n");
}

// Affichage d'un PDU
void print_SOCKET(mic_tcp_sock sock, int nature) // nature 1 --> Serveur, nature 0 --> client
{
	printf("***************SOCKET*****************\n");
	if(nature)
		printf("SERVEUR\n");
	else
		printf("CLIENT\n");

	printf("Adresse IP : %s - Port : %d\n", sock.addr.ip_addr, sock.addr.port); 	
	printf("Etat : %d\n", sock.state);
	printf("Numero de sequence : %d\n", sock.n_seq);
	printf("**********************************\n\n");
}


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

// behaviour = 1 -> target has to be connected (normal mode)
// behaviour = 0 -> target can be in any mode (for connect())
int mic_tcp_first_available_for_ack_from_addr(mic_tcp_sock_addr addr)
{ // get the first socket that is waiting for an ack on addr
	int i;
	for(i=0;i<__TCP_MIC_current;++i)
	{
		//printf("%.*s :::: %.*s\n", sizeof(mic_tcp_sock_addr), &__MIC_TCP_sock_buffer[i].addr, sizeof(mic_tcp_sock_addr),  &addr);
		//if((memcmp(&__MIC_TCP_sock_buffer[i].addr, &addr, sizeof(mic_tcp_sock_addr))) && (__MIC_TCP_sock_buffer[i].state == WAIT_FOR_ACK))
		
		// TODO : refaire
		if(((__MIC_TCP_sock_buffer[i].addr.port == addr.port) || (addr.port == 0)) && ((1) || (__MIC_TCP_sock_buffer[i].state == CONNECTED)))
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
signals to the socket that a PDU has been received
	received = 0 -> really received
	received = 1 -> no ack has been received
*/
void mic_tcp_add_pdu_received(int fd, int received)
{
	int id = mic_tcp_get_id_from_fd(fd);
	mic_tcp_sock *socket = &__MIC_TCP_sock_buffer[id];

	if(socket->lastReceivedSize < __TCPMIC_PDURECEIVEDBUFSIZE)
	{
		socket->lastReceived[socket->lastReceivedSize] = received;
		++socket->lastReceivedSize;
	}
	else
	{
		socket->lastReceived[socket->lastReceivedCurrent] = received;
		socket->lastReceivedCurrent = (socket->lastReceivedCurrent+1)%__TCPMIC_PDURECEIVEDBUFSIZE;
	}
}

/*
returns the real loss rate for a given socket
*/
float mic_tcp_get_effective_loss_rate(int fd)
{
	int total = 0;
	int id = mic_tcp_get_id_from_fd(fd);
	mic_tcp_sock *socket = &__MIC_TCP_sock_buffer[id];
	int i;
	if(socket->lastReceivedSize == 0)
	{
		return 1;
	}
	for(i=0;i<socket->lastReceivedSize;++i)
	{
		total += socket->lastReceived[i];
	}
	return (float)total / (float)__TCPMIC_PDURECEIVEDBUFSIZE;
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
	__MIC_TCP_sock_buffer[__TCP_MIC_current].lastReceivedSize = 0;
	__MIC_TCP_sock_buffer[__TCP_MIC_current].lastReceivedCurrent = 0;


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

	mic_tcp_pdu pdu_syn;
	pdu_syn.payload.size = 0;
	mic_tcp_sock_addr syn_addr;

	while(IP_recv(&pdu_syn, &syn_addr, __TCPMIC_WAIT_CONNEXION_TICK) == -1);

	printf("Reception du SYN\n");
	print_PDU(pdu_syn);

	__MIC_TCP_sock_buffer[id].state = SYN_RECEIVED;
	__MIC_TCP_sock_buffer[id].n_seq = pdu_syn.header.seq_num^1;

	// chosen loss rate behaviour
	float final_loss_rate = __TCPMIC_DEFAULT_SERVER_LOSS_RATE;

	if(pdu_syn.payload.size == sizeof(float))
	{
		memcpy(&final_loss_rate, pdu_syn.payload.data, sizeof(float));
	}
	// end chosen loss rate behaviour

	mic_tcp_pdu pdu_syn_ack;
	pdu_syn_ack.header = mic_tcp_build_header(TCPMIC_SYN|TCPMIC_ACK, pdu_syn.header.seq_num, __MIC_TCP_sock_buffer[id].n_seq, __MIC_TCP_sock_buffer[id].addr.port, __MIC_TCP_sock_buffer[id].addr.port);
	pdu_syn_ack.payload.data = (char*)&final_loss_rate;
	pdu_syn_ack.payload.size = sizeof(float);

	mic_tcp_pdu pdu_ack;
	pdu_ack.payload.size = 0;
	mic_tcp_sock_addr ack_addr;

	__MIC_TCP_sock_buffer[id].n_seq = pdu_syn.header.seq_num^1;

	int recv_ret;

	/*do
	{
		printf("ack was %d \n", pdu_ack.header.ack);

		if(IP_send(pdu_syn_ack, __MIC_TCP_sock_buffer[id].addr) == -1)
		{
			continue;
		}

		printf("ENVOI du SYN-ACK\n");
		print_PDU(pdu_syn_ack);
		//sleep(1);

		recv_ret = IP_recv(&pdu_ack, &ack_addr, __TCPMIC_WAIT_CONNEXION_TICK);

		printf("return : %d\n", recv_ret);

	}while(recv_ret == -1 || pdu_ack.header.ack != 1); // while the PDU we receive is not an ack
	// we send SYN_ACK as long as we don't receive ACK*/

	do
	{
		if(IP_send(pdu_syn_ack, __MIC_TCP_sock_buffer[id].addr) == -1)
		{
			continue;
		}
		sleep(1);
	}while(__MIC_TCP_sock_buffer[id].state == WAIT_FOR_ACK);

	printf("Reception du ACK\n");
	print_SOCKET(__MIC_TCP_sock_buffer[id],1);
	//print_PDU(pdu_ack);
	
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

	int i;
	float submittedLossRate = __TCPMIC_DEFAULT_CLIENT_LOSS_RATE;
	float receivedLossRate;

	mic_tcp_pdu pdu_syn;
	pdu_syn.header = mic_tcp_build_header(TCPMIC_SYN, 0, __MIC_TCP_sock_buffer[id].n_seq, __MIC_TCP_sock_buffer[id].addr.port, __MIC_TCP_sock_buffer[id].addr.port);
	pdu_syn.payload.data = (char*)(&submittedLossRate);
	pdu_syn.payload.size = sizeof(float);


	mic_tcp_pdu pdu_ack;
	pdu_ack.payload.size = 0;
	
	int send_ret;

	mic_tcp_pdu pdu_syn_ack;
	pdu_syn_ack.payload.size = 0;
	mic_tcp_sock_addr syn_ack_addr;

	for(i=0;i<__TCPMIC_CLIENT_SEND_SYN_ATTEMPT;++i)
	{
		send_ret = IP_send(pdu_syn, __MIC_TCP_sock_buffer[id].addr);
		__MIC_TCP_sock_buffer[id].state = SYN_SENT;

		printf("Sending Syn\n");
		//print_PDU(pdu_syn);
		if(send_ret == -1)
		{ // if IP_send didn't send
			continue;
		}

		if((IP_recv(&pdu_syn_ack, &syn_ack_addr, __TCPMIC_WAIT_ACK_TIME) == -1))
		{
			continue;
		}

		if(pdu_syn_ack.header.syn != 1 || pdu_syn_ack.header.ack != 1)
		{
			printf("This is not a SYNACK\n");
			continue;
		}

		if(pdu_syn_ack.header.ack_num != pdu_syn.header.seq_num || pdu_syn_ack.header.seq_num != (pdu_syn_ack.header.ack_num^1))//|| pdu_syn_ack.header.seq_num != (pdu_syn_ack.header.ack_num^1))
		{ // for some reason we received a bad SYN_ACK
			continue;
		}

		printf("Receiving SynACK\n");
		print_PDU(pdu_syn_ack);

		__MIC_TCP_sock_buffer[id].n_seq = __MIC_TCP_sock_buffer[id].n_seq^1;

		if(pdu_syn_ack.payload.size == sizeof(float))
		{ // the server suggests an other loss rate
			memcpy(&receivedLossRate, pdu_syn_ack.payload.data, sizeof(float));
		}
		else
		{ // if the server doesn't send a loss rate, he implicitly accept ours
			receivedLossRate = submittedLossRate;
		}

		if(receivedLossRate != submittedLossRate)
		{ // the server doesn't agree
			// by default, the client accepts anything from the server
			submittedLossRate = receivedLossRate; // we accept the server's loss rate
		}

		// we build the PDU according to the SYN_ACK we received
		pdu_ack.header = mic_tcp_build_header(TCPMIC_ACK, pdu_syn_ack.header.seq_num, 0, /*__MIC_TCP_sock_buffer[id].addr.port*/1234, /*__MIC_TCP_sock_buffer[id].addr.port*/1234);


		if(IP_send(pdu_ack, __MIC_TCP_sock_buffer[id].addr) == -1)
			printf("Erreur lors de l'envoi du ACK\n");
		// we send the ack without any test, since any error would result on the server sending again SYN_ACK and thus the client would ACK back

		printf("Sending ACK\n");
		print_PDU(pdu_ack);
		
		__MIC_TCP_sock_buffer[id].state = CONNECTED;
		break;
	}

	if(__MIC_TCP_sock_buffer[id].state != CONNECTED)
	{ // failed to connect
		__MIC_TCP_sock_buffer[id].state = BOUND;
		return -1;
	}

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
		usleep(5); // wait 500 usec (arbitrary value)
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
		ret = IP_recv(&pduAck, &addr, __TCPMIC_WAIT_ACK_TIME);
		if(ret != -1)
		{
			if(pduAck.header.ack && (pduAck.header.ack_num == __MIC_TCP_sock_buffer[id].n_seq))
			{
				mic_tcp_add_pdu_received(mic_sock, 0);
				break;
			}
		}

		if(mic_tcp_get_effective_loss_rate(mic_sock) <= __TCPMIC_TOLERATED_LOSS)
		{
			mic_tcp_add_pdu_received(mic_sock, 1);
			break;
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
		
			memcpy(mesg, content.data, content.size);
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

	int id = mic_tcp_get_id_from_fd(socket);

	if(id == -1)
	{
		return -1;
	}

	if(__MIC_TCP_sock_buffer[id].state != CONNECTED)
	{
		return -1;
	}



	mic_tcp_pdu pdu_fin;
	pdu_fin.header = mic_tcp_build_header(TCPMIC_FIN, 0, __MIC_TCP_sock_buffer[id].n_seq, __MIC_TCP_sock_buffer[id].addr.port, __MIC_TCP_sock_buffer[id].addr.port);
	pdu_fin.payload.size = 0;

	mic_tcp_pdu pdu_fin_ack;
	pdu_fin_ack.payload.size = 0;

	mic_tcp_sock_addr fin_ack_addr;
	
	
	int send_ret;
	int i;

	for(i=0;i<__TCPMIC_CLIENT_SEND_FIN_ATTEMPT;++i)
	{
		send_ret = IP_send(pdu_fin, __MIC_TCP_sock_buffer[id].addr);
		__MIC_TCP_sock_buffer[id].state = FIN_SENT;

		printf("Sending FIN\n");
		//print_PDU(pdu_syn);
		if(send_ret == -1)
		{ // if IP_send didn't send
			continue;
		}

		if((IP_recv(&pdu_fin_ack, &fin_ack_addr, __TCPMIC_WAIT_ACK_TIME) == -1))
		{
			continue;
		}

		if(pdu_fin_ack.header.fin != 1 || pdu_fin_ack.header.ack != 1)
		{
			printf("This is not a FINACK\n");
			continue;
		}

		if(pdu_fin_ack.header.ack_num != pdu_fin.header.seq_num || pdu_fin_ack.header.seq_num != (pdu_fin_ack.header.ack_num^1))//|| pdu_fin_ack.header.seq_num != (pdu_fin_ack.header.ack_num^1))
		{ // for some reason we received a bad FIN_ACK
			continue;
		}

		printf("Receiving FINACK\n");
		print_PDU(pdu_fin_ack);

		__MIC_TCP_sock_buffer[id].state = CLOSING;
		break;

	}

	if(__MIC_TCP_sock_buffer[id].state != CLOSING)
	{
		return(-1);
	} 
	else
	{

		//TO DO : Close the socket - Destroy structure...

		return 0;

	}
	



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

	if(pdu.header.syn == 1)
	{
		printf("ignoring syn received in process_received_PDU\n");
	}
	else if(pdu.header.fin == 1)
	{
		mic_tcp_header ack_header = mic_tcp_build_header(TCPMIC_ACK|TCPMIC_FIN, pdu.header.seq_num, 0/*unused*/, __MIC_TCP_sock_buffer[id_socket].addr.port, __MIC_TCP_sock_buffer[id_socket].addr.port);
		mic_tcp_pdu ack_pdu;
		ack_pdu.header = ack_header;
		ack_pdu.payload.data = NULL;
		ack_pdu.payload.size = 0;

		IP_send(ack_pdu, addr);

		//TO DO : clean socket + destroy structure
	}
	else if(pdu.header.ack != 1) // if it is not an ack pdu nor a syn
	{
		mic_tcp_header ack_header = mic_tcp_build_header(TCPMIC_ACK, pdu.header.seq_num, 0/*unused*/, __MIC_TCP_sock_buffer[id_socket].addr.port, __MIC_TCP_sock_buffer[id_socket].addr.port);
		mic_tcp_pdu ack_pdu;
		ack_pdu.header = ack_header;
		ack_pdu.payload.data = NULL;
		ack_pdu.payload.size = 0;

		// On envoie l'acquittement du PDU reçu avec N°ACK = N°SEQ_reçu
		printf("envoi de l'acquittement, on acquitte %d \n", pdu.header.seq_num);
		printf("on attendait le numero : %d \n", __MIC_TCP_sock_buffer[id_socket].n_seq);
		IP_send(ack_pdu, addr);


		if(pdu.payload.data == 0 || pdu.header.syn == 1) 
		{
			printf("Packet vide ou syn recu\n");
		}
		else
		{
			// On délivre les datas si les numéros coincident + on met a jour numéro seq
			if(pdu.header.seq_num == __MIC_TCP_sock_buffer[id_socket].n_seq)
			{
				app_buffer_put(pdu.payload);
				printf("%s\n", pdu.payload.data);	
				__MIC_TCP_sock_buffer[id_socket].n_seq ^= 1;
			}
			else
			{
				printf("Mauvais numero de sequence\n");
			}
		}
	}
	else if(__MIC_TCP_sock_buffer[id_socket].state == WAIT_FOR_ACK)
	{ // if we wait for an ack and receive an ack :
		if(__MIC_TCP_sock_buffer[id_socket].n_seq == pdu.header.ack_num)
		{
			__MIC_TCP_sock_buffer[id_socket].n_seq ^= 1;
			__MIC_TCP_sock_buffer[id_socket].state = CONNECTED;
		}
		else
		{
			printf("bad ack received\n");
		}
	}
}

