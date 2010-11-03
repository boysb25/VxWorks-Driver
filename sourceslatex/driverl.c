/**
 * Auteurs : Paul ADENOT & Etienne BRODU
 * Ce fichier fournit l'impl�mentation d'un pilote de p�riph�rique VxWorks, capable
 * de fournir un moyen d'interroger des capteurs sur un bus CAN.
 */


#include "driver.h"
#include <vxWorks.h>
#include <string.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <stdio.h>
#include <semLib.h>
#include <timers.h>
#include <time.h>
#include <sysLib.h>
#include <msgQLib.h>
#include <taskLib.h>
#include <errnoLib.h>

/**
 * Constantes internes.
 */
/* Nom d'un p�riph�rique. Doit �tre utilis� avec sprintf pour
 * indiquer son num�ro */
#define DEVICE_BASENAME "/dev/capteur%d"

/* Nombre maximal de p�riph�rique installables */
#define DEVICE_MAX_COUNT (15)

#define DISPATCHER_PRIORITY 200

/***
 * == Globaux
 ***/
/** driver_id vaut -1 si le driver n'est pas install�.
 * Il est �gal au num�ro du driver sinon.
 */
static int driver_id = -1;
/**
 * Buffer pour les messages. Chaque capteur a la place pour
 * un seul message.
 */
static Message table_buffer[DEVICE_MAX_COUNT];
/**
 * Table d'information sur les p�riph�riques.
 */
static PEDEV table_capt[DEVICE_MAX_COUNT];
/**
 * S�maphore d'exclusion mutuelle pour la table des capteurs.
 */
static SEM_ID mut_table_capt;
/**
 * S�maphore d'exclusion mutuelle pour les buffers de messages.
 */
static SEM_ID mut_table_buffer;

/**
 * File de messages receptionn�s par le handler d'it.
 */
static MSG_Q_ID msgQ_dispatcher;

static int id_pe_task_dispatcher;

/**
 * Fausse m�moire de la carte r�seau pour le bus can.
 */
static struct 
{
	unsigned char address;
	char data[MAX_DATA_SIZE];
} mock_can_device;

static Message msg_buffer;

/***
 * /////////////////////////////////////////////////////////////////
 * /// Fonctions du driver : impl�mentation des appels syst�mes. ///
 * /////////////////////////////////////////////////////////////////
 ***/

/**
 * 
 * @param desc
 * @param remainder
 * @param flag
 * @return desc
 */
static int pe_open(PEDEV* desc, char* remainder, int flag) 
{
	if( *remainder != '\0' )
	{
		errnoSet(EARG);
		return ERROR;
	}
	
	if( flag != O_RDONLY )
	{
		errnoSet(EARG);
		return ERROR;
	}

	// On ouvre le capteur
	semTake(mut_table_capt, WAIT_FOREVER);
	
	switch (desc->specific.state)
	{
		case opened :
			semGive(mut_table_capt);
			errnoSet(EALREADYOPENED);
			return ERROR;
			
		case notcreated :
			semGive(mut_table_capt);
			errnoSet(ENEXIST);
			return ERROR;
			
		default:
			break;
	}
	
	desc->specific.state = opened;
	semGive(mut_table_capt);

	return ((int) desc);
}

/**
 * 
 * @param desc
 * @param remainder
 * @param flag
 * @return OK
 */
static int pe_close(PEDEV* desc, char* name)
{
	// On ferme le capteur
	semTake(mut_table_capt, WAIT_FOREVER);
	if(desc->specific.state == opened)
	{
		desc->specific.state = closed;
		semGive(mut_table_capt);
		return OK;
	}
	else
	{
		semGive(mut_table_capt);
		errnoSet(ENOTOPENED);
		return ERROR;
		
	}
}

/**
 * 
 * @param desc
 * @param buff
 * @param nBytes
 * @return
 */
static int pe_read (PEDEV* desc, char* buff, int nBytes) 
{
	int device;
		
	if(nBytes < 0)
	{
		errnoSet(EARG);
		return ERROR;
	}
	
	/*
	 * Pour trouver l'index du capteur dans le tableau de capteur,
	 * 2 solutions sont possible :
	 * 
	 * - chercher une correspondance entre l'adresse
	 * renvoy� par le descripteur de fichier et chaucun des capteurs du tableau de capteur.
	 * - calculer l'indice en soustrayant l'adresse du descripteur de fichier a l'adresse
	 * du tableau de capteur.
	 * 
	 * Pour une question d'efficacit�, nous avons pr�f�r� calculer l'indice,
	 * cependant, cette methode ne peut pas �tre consid�r� comme s�r, nous avons donc laiss�
	 * l'autre solution imagin�.
	 */
	
	//semTake(mut_table_capt, WAIT_FOREVER);
	// On cherche si
	/*for(i=0; i < DEVICE_MAX_COUNT; ++i)
	{
		if(table_capt[i].specific.state == opened &&
		   table_capt[i].specific.address == desc->specific.address)
		{
	
			semTake(mut_table_buffer, WAIT_FOREVER);
			memcpy(buff, &(table_buffer[i]), nBytes);
			semGive(mut_table_buffer);
			
			break;
		}
	}
	semGive(mut_table_capt);
	*/
	
	// On clacule l'index du capteur en comparant l'adresse de desc a celle du tableau.
	device = ((int)desc - (int)&(table_capt))/sizeof(PEDEV);
		
	semTake(mut_table_buffer, WAIT_FOREVER);
	if(table_buffer[device].id == -1)
	{
		semGive(mut_table_buffer);
		errnoSet(ENOAVAIL);
		return ERROR;
	}
	memcpy(buff, &(table_buffer[device]), nBytes);
	semGive(mut_table_buffer);
	
	return sizeof(Message);
}

/**
 * 
 * @param desc
 * @param request
 * @param value
 * @return OK en cas de succ�s, ERROR sinon.
 */
static int pe_ioctl_change_cpt(PEDEV* desc, int request, int value)
{
	int i;
	if(value >= 0 && value <= 255)
	{
		semTake(mut_table_capt, WAIT_FOREVER);

		for(i=0; i < DEVICE_MAX_COUNT; ++i)
		{
			if(table_capt[i].specific.state == opened &&
			   table_capt[i].specific.address == value)
			{
				errnoSet(ECPTALREADYUSED);
				return ERROR;
			}
		}
		
		desc->specific.address = value;
		semGive(mut_table_capt);
		return OK;
	}
	else
	{
		errnoSet(EARG);
		return ERROR;
	}	
}

/**
 * 
 * @param desc
 * @param request
 * @param value
 * @return OK en cas de succ�s, ERROR sinon.
 */
static int pe_ioctl (PEDEV* desc, int request, int value)
{
	switch (request)
	{
		case SET_CPT_ADDRESS:
			return pe_ioctl_change_cpt(desc, request, value);
		default:
			errnoSet(EARG);
			return ERROR;
	}
}

/**
 * Cette fonction est appel�e quand un message est disponible sur le r�seau.
 * Elle num�rote et date le message, et le place dans le buffer correspondant a son
 * addresse, si un device logiciel est associ� � un capteur.
 * Sinon, le message est perdu.
 */
void pe_interrupt_handler()
{
	/* Les id des messages sont globales au driver */
	static unsigned id_msg = 0;
	Message msg;
	
	msg.id = id_msg++;
	// On fait passer l'adresse par la date non initialis� pour �viter des champs de m�moire suppl�mentaire inutile.
	msg.date.tv_nsec = mock_can_device.address;
	strncpy(msg.msg, mock_can_device.data, MAX_DATA_SIZE);
	
	msgQSend( msgQ_dispatcher, (char*)&msg, sizeof(msg), WAIT_FOREVER, MSG_PRI_NORMAL);
}

void pe_dispatcher()
{
	int i;
	Message msg;
	
	for(;;)
	{
		msgQReceive(msgQ_dispatcher, (char*)&msg, sizeof(msg), WAIT_FOREVER);
		
		semTake(mut_table_capt, WAIT_FOREVER);
		// On cherche si
		for(i=0; i < DEVICE_MAX_COUNT; ++i)
		{
			if(table_capt[i].specific.state == opened &&
			   table_capt[i].specific.address == msg.date.tv_nsec)
			// On fait passer l'adresse par la date non initialis� pour �viter des champs de m�moire suppl�mentaire inutile.
			{
				// Trouv� : on ne drop pas le message.
				clock_gettime(CLOCK_REALTIME, &(msg.date));

				semTake(mut_table_buffer, WAIT_FOREVER);
				memcpy((char*)&(table_buffer[i]), (char*)&msg, sizeof(Message));
				semGive(mut_table_buffer);
				
				break;
			}
		}
		semGive(mut_table_capt);
	}
}


/**
 * @brief D�truire les ressources syst�mes.
 * @return OK en cas de succ�s, ERROR sinon.
 */
int pe_cleanup_resources()
{
	int errtest = 0;
	
	// Destruction des ressources.
	if(semDelete(mut_table_capt))
	{
		printf("Erreur : Probl�me de destruction du s�maphore pour table_capt.\n");
		errtest = 1;
	}
	if(semDelete(mut_table_buffer))
	{
		printf("Erreur : Probl�me de destruction du s�maphore pour table_buffer.\n");
		errtest = 1;
	}
	if(msgQDelete(msgQ_dispatcher))
	{
		printf("Erreur : mauvais num�ro de file de message.\n");
		errtest = 1;
	}	
	
	if(taskDelete(id_pe_task_dispatcher))
	{
		printf("Erreur : mauvais num�ro de tache pour la tache dispatcher.\n");
		errtest = 1;
	}
	
	if (errtest)
	{
		return ERROR;
	}
	return OK;
}

/***
 * ///////////////////////////////////////////////////////
 * /// Fonction d'administration du driver : interface ///
 * ///////////////////////////////////////////////////////
 *
 * Ces fonctions permettent d'installer / d�sinstaller le pilote, et
 * d'enlever / ajouter des p�riph�riques.
 * 
 * Elles encapsulent les appels classiques de l'ios (iosDrvInstall, etc.),
 * et doivent �tre utilis�es pour manipuler ce driver.
 ***/

/**
 * @brief Installation du p�riph�rique.
 * 
 * @param dev_count Le nombre de p�riph�riques � cr�er.
 * 
 * @return OK si la fonction s'est execut� normalement, ERROR sinon.
 */
int pe_driverInstall(int dev_count)
{
	/* iteration pour la cr�ation des p�riph�riques */
	int i = 0;
	struct timespec initial_time = {0 , 0};
	
	
	if(dev_count > DEVICE_MAX_COUNT)
	{
		errnoSet(ETOOMUCHDEV);
		return ERROR;
	}
	
	if(driver_id != -1)
	{
		errnoSet(EINSTALLED);
		return ERROR;
	}
	
	/* Driver installation */
	driver_id = iosDrvInstall(pe_open,pe_close, pe_open, pe_close, pe_read, (FUNCPTR) NULL, pe_ioctl);
	if(driver_id < 0)
	{
		errnoSet(EUNKNOW);
		return ERROR;
	}
	
	/* Cr�ation du s�maphore d'exclusion mutuelle pour les deux tables (table_capt
	 * et table_buffer, qui sont susceptibl
	 * Les t�ches s'enfilent dans l'ordre d'arriv�e avec SEM_Q_FIFO, et 
	 * SEM_DELETE_SAFE garantie que la t�che ayant verrouill� un s�maphore ne soit 
	 * pas d�truite avant de le lib�rer.
	 */
	mut_table_capt = semMCreate(SEM_Q_FIFO | SEM_DELETE_SAFE);
	
	if(mut_table_capt == NULL)
	{
		return ERROR;
	}
	
	mut_table_buffer = semMCreate(SEM_Q_FIFO | SEM_DELETE_SAFE);
	
	if(mut_table_buffer == NULL)
	{
		return ERROR;
	}
	
	/* Cr�ation de la file de message de communication entre le serveur d'interuption
	 * et la t�che dispatcher
	 */
	msgQ_dispatcher = msgQCreate(10, sizeof(Message), MSG_Q_FIFO );
	
	if(msgQ_dispatcher == NULL)
	{
		return ERROR;
	}	
	
	/* Cr�ation de la t�che dispatcher
	 */
	id_pe_task_dispatcher = taskSpawn("pe_task_dispatcher", DISPATCHER_PRIORITY, 0, 512, (FUNCPTR) pe_dispatcher, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	
	if(id_pe_task_dispatcher == ERROR)
	{
		return ERROR;
	}

	for(i=0 ; i < dev_count; ++i)
	{
		if(pe_deviceAdd(i) == ERROR)
		{
			pe_cleanup_resources();
			errnoSet(ECANNOTADD);
			return ERROR;
		}
	}
	
	for(; i < DEVICE_MAX_COUNT; i++)
	{
		table_capt[i].specific.address = -1;
		table_capt[i].specific.state = notcreated;
		table_buffer[i].id = -1;
	}
	
	/* Initialisation de l'horloge syst�me : elle est plac�e arbitrairement
	 * � 0:0 � l'installation du pilote. */
	sysClkRateSet(100);
	clock_settime(CLOCK_REALTIME, &initial_time);
	
	return driver_id;
}

/**
 * @brief Retirer le driver.
 * @return OK en cas de succ�s, ERROR sinon.
 */
int pe_driverUninstall()
{
	int i;
	if(driver_id != -1)
	{
		/* L'appel ne renvoit pas d'erreur bien que des capteurs soient ouverts.
		 * Nous v�rifions donc qu'aucun capteur ne soit ouvert.
		 */
		
		semTake(mut_table_capt, WAIT_FOREVER);
		for(i=0; i < DEVICE_MAX_COUNT; ++i)
		{
			if(table_capt[i].specific.state == opened)
			{
				errnoSet(ECPTBUSY);
				return ERROR;
			}
		}
		semGive(mut_table_capt);
		
		if(iosDrvRemove(driver_id, FALSE) == ERROR)
		{
			// The driver has open files.
			errnoSet(ECPTBUSY);
			return ERROR;
		}
		
		driver_id = -1;
		
		if(pe_cleanup_resources() == ERROR)
		{
			return ERROR;
		}
	}
	else
	{
		errnoSet(ENINSTALLED);
		return ERROR;
	}
	
	return OK;
}

/**
 * @brief Ajouter un p�riph�rique.
 * @param i
 * @return OK en cas de succ�s, ERROR sinon.
 */
int pe_deviceAdd(int i)
{
	// Buffer pour le nom de p�riph�rique.
	char dev_name[NAME_SIZE];

	if(driver_id == -1)
	{
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}
	
	sprintf(dev_name, DEVICE_BASENAME, i);
	if(iosDevAdd((DEV_HDR*)&(table_capt[i]),dev_name,driver_id) == ERROR)
	{
		errnoSet(ECANNOTADD);
		return ERROR;
	}
	
	semTake(mut_table_capt, WAIT_FOREVER);
	// Le capteur est ferm�
	table_capt[i].specific.state = closed;
	// Il n'a pas encore de capteur associ�, au niveau CAN.
	// utiliser ioctl pour lui en founir une.
	table_capt[i].specific.address = -1;
	semGive(mut_table_capt);
	return OK;
}

/**
 * @brief Enlever un p�riph�rique.
 * @param i L'index du device � enlever
 * @return OK pour un succ�s, ERROR sinon. 
 */
int pe_deviceRemove(int i)
{
	semTake(mut_table_capt, WAIT_FOREVER);
	if(i < DEVICE_MAX_COUNT && table_capt[i].specific.state != notcreated)
	{	
		iosDevDelete((DEV_HDR*)&(table_capt[i]));
		table_capt[i].specific.state = notcreated;
		table_capt[i].specific.address = -1;
		semGive(mut_table_capt);
		
		semTake(mut_table_capt, WAIT_FOREVER);
		table_buffer[i].id = -1;
		semGive(mut_table_buffer);
		
		return OK;
	}
	else
	{
		// Le capteur n'a pas �t� cr�e :
		// " If the device was never added to the device list, 
		//   unpredictable results may occur."
		semGive(mut_table_capt);
		errnoSet(ENEXIST);
		return ERROR;
	}
}

/**
 * Cette fonction mime le comportement du hardware :
 * Elle place un message dans le faux buffer de la carte r�seau,
 * et simule une interruption, en appelant le handler.
 * 
 * @param address L'adresse du capteur o� est envoy�e le message.
 */
void hardware_mockup(int address, char* data)
{
	strncpy(mock_can_device.data, data, MAX_DATA_SIZE);
	mock_can_device.address = address;
	pe_interrupt_handler();
}
