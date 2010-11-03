/**
 * Auteurs : Paul ADENOT & �tienne Brodu.
 * Ce fichier fournit la suite de test pour le pilote
 * impl�ment� dans le fichier driver.c
 */

#include "driver.h"
#include <vxWorks.h>
#include <errnoLib.h>
#include <stdio.h>
#include <string.h>

/* �crit un message d'erreur sur la sortie d'erreur */
#define ERR_MSG(x) printf("Test %d failed. errno : %d\n",(x), errnoGet());

/**
 * Test 1 - Installation d'un driver
 * Description
 * Installer le driver alors qu'il n'est pas install�
 * Resultat attendu
 * La valeur de retour doit �tre positive, et correspond au num�ro du driver. Il doit �tre possible
 * de le retrouver en utilisant la fonction iosDrvShow.
 **/
int test_1()
{
	int success = 0;
	if(pe_driverInstall(10) >= 0)
	{
		success++;
		iosDrvShow();
		getchar();
		// Call this to clean the system.
	}
	else
	{
		ERR_MSG(1);
	}
	pe_driverUninstall();
	return success;
}

 /**
 * Test 2 - Installation d'un driver d�j� install�
 * Description
 * Installer le driver alors qu'il est d�j� install� : appeler deux fois iosDrvInstall.
 * Resultat attendu
 * La premi�re installation doit bien se passer (valeur de retour positive). Le second appel de
 * iosDrvInstall doit renvoyer ERROR.
 **/
int test_2()
{
	int success = 0;
	if(pe_driverInstall(10))
	{
		if( pe_driverInstall(10) == -1)
		{
			success++;
		}
		else
		{
			ERR_MSG(2)
		}
	}
	else
	{
		ERR_MSG(2)
	}
		
	// Call this to clean the system.
	pe_driverUninstall();
	return success;
}
 /**
 * Test 3 - Retrait d'un driver
 * Description
 * Utilisation de la fonction iosDrvRemove, alors que le pilote est install� sur le syst�me, et qu'il
 * n'est pas utilis�.
 * Resultat attendu
 * La valeur de retour doit �tre �gale � OK.
 **/
int test_3()
{
	int success = 0;
	pe_driverInstall(10);
	if(pe_driverUninstall() == OK)
	{
		success++;
	}
	else
	{
		ERR_MSG(3);
	}
	return success;
}
 /**
 * Test 4 - Retrait du driver alors qu'il n'est pas install�
 * Description
 * Utilisation de la fonction iosDrvRemove, alors que le pilote n'est pas install� sur le syst�me.
 * Resultat attendu
 * La valeur de retour doit �tre ERROR.
 **/
int test_4()
{
	int success = 0;
	if(pe_driverUninstall() == ERROR)
	{
		success++;
	}
	else
	{
		ERR_MSG(4)
	}
	
	return success;
}

/**
 * Test 5 - Retrait du driver alors qu'un p�riph�rique est ouvert
 * Description
 * Alors qu'un capteur a �t� ouvert en lecture, retirer le driver, � l'aide de la fonction iosDrvRemove.
 * Resultat attendu
 * La fonction doit retourner ERROR, et errno doit �tre positionn� � ECPTBUSY. Le driver ne doit
 * pas �tre retir�.
 */
int test_5()
{
	int success = 0;
	int fd = 0;
	Message buffer;

	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur5", O_RDONLY, 0);	
	ioctl(fd, SET_CPT_ADDRESS, 42);
	hardware_mockup(42, "test 5\0");


	// l'attente est obligatoire sinon le message n'est pas encore trait�.
	sleep(1);
	read(fd,(char*)&(buffer), sizeof(buffer));
	
	if(strncmp(buffer.msg, "test 5\0", 7) != 0)
	{
		printf("Erreur a la lecture du message\n");
	}
	
	if( fd != ERROR )
	{
		// Tentative de retrait du driver : cela doit �tre un �chec.
		if(pe_driverUninstall() == ERROR)
		{
			if(errnoGet() == ECPTBUSY)
				success++;
			else
				printf("Pas le bon code errno : test_5\n");
		}
		else
		{
			printf("driver enleve avec succes : Echec\n");
		}
		
		close(fd);
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}	
	pe_driverUninstall();
		
	return success;
}

/**
 * Test 6 - Ajout d'un p�riph�rique
 * Description
 * Utilisation de la fonction iosDevAdd, une seule fois, avec des param�tre valides.
 * Resultat attendu
 * La valeur de retour doit �tre OK, le p�riph�rique doit �tre trouvable en utilisant iosDevFind, qui
 * ne doit pas renvoyer NULL.
 **/
int test_6()
{
	int success = 0;
	int fd = 0;

	// on n'installe pas de device par d�faut.
	pe_driverInstall(0);
	
	// On utilise la proc�dure qui encapsule iosDevAdd.
	if(pe_deviceAdd(0) == OK)
	{
		if(iosDevFind("/dev/capteur0",NULL) != NULL)
		{
			success++;
		}
		else
		{
			ERR_MSG(6);
		}
	}
	else
	{
		ERR_MSG(6);
	}
	
	pe_driverUninstall();
	
	return success;
}
/**
 * Test 7 - Retrait d'un p�riph�rique
 * Description
 * Utilisation de la fonction iosDevDelete, avec des param�tres valides.
 * Resultat attendu
 * Il ne doit plus �tre possible d'ouvrir le p�riph�rique : un appel � open sur ce p�riph�rique doit
 * �chouer (il doit un nombre n�gatif), et errno doit �tre positionn� � ENOENT.
 **/
int test_7()
{
	int success = 0;
	int fd = 0;

	// on n'installe pas de device par d�faut.
	
	pe_driverInstall(0);
	
	// On utilise la proc�dure qui encapsule iosDevAdd.
	if(pe_deviceAdd(0) == OK)
	{
		if(pe_deviceRemove(0) == OK)
		{
			if(open("/dev/capteur0", O_RDONLY, 0) == ERROR)
			{
				if(errnoGet() == ENOENT)
				{
					success++;
				}
				else
				{
					ERR_MSG(7);
				}
			}
			else
			{
				printf("Il est possible d'ouvrir le device alors qu'il a �t� retir� : 7\n");
			}
		}
		else
		{
			printf("L'ajout a �chou� : 7\n");
		}
	}
	else
	{
		printf("Erreur a l'ajout du peripherique\n");
	}

	pe_driverUninstall();
	
	return success;
}
/**
 * Test 8 - Ajout d'un p�riph�rique alors que 15 p�riph�riques ont �t� ajout�s.
 * Description
 * Utilisation de la fonction iosDevAdd sur un p�riph�rique d�j� ajout�.
 * L'appel � iosDevAdd doit provoquer une erreur, et renvoyer ERROR.
 **/
int test_8()
{
	int success = 0;
	
	if(pe_driverInstall(15) == ERROR)
	{
		return success;
	}
		
	if(pe_deviceAdd(0) == ERROR)
	{
		success++;
	}
	
	pe_driverUninstall();
	
	return success;
}
/**
 * Test 9 - Ouverture d'un capteur
 * Description
 * Appeler open sur un capteur valide (le fichier existe et est accessible en �criture), avec des options
 * valide (O_RDONLY), une seule fois.
 * Resultat attendu
 * La valeur de retour doit �tre un entier positif.
 **/
int test_9()
{
	int success = 0;
	int fd = 0;

	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDONLY, 0);
	if( fd == ERROR )
	{
		printf("Erreur d'ouverture du peripherique\n");
	}
	else
	{
		success++;
		if(close(fd))
		{
			printf("Erreur pendant la fermeture du capteur\n");
		}
	}
	
	if(pe_driverUninstall())
	{
		printf("Erreur de d�sinstallation du driver\n");
	}
	
	return success;
}
/**
 * Test 10 - Ouverture d'un capteur d�j� ouvert
 * Description
 * Appeler open sur un capteur valide (le fichier existe, et est accessible en lecture), alors qu'il vient
 * d'�tre ouvert avec succ�s.
 * Resultat attendu
 * open doit renvoyer ERROR, et errno doit �tre positionn� � EALREADYOPENED.
 **/
int test_10()
{
	int success = 0;
	int fd1 = 0;
	int fd2 = 0;
	
	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd1 = open("/dev/capteur0", O_RDONLY, 0);
	if( fd1 == ERROR )
	{
		printf("Erreur d'ouverture du peripherique\n");
		if( pe_driverUninstall() == ERROR )
		{
			printf("Erreur de d�sinstallation du driver\n");
		}
		return success;
	}
	
	fd2 = open("/dev/capteur0", O_RDONLY, 0);
	if( fd2 == ERROR )
	{
		success++;
	}
	
	close(fd2);
	
	if(close(fd1))
	{
		printf("Erreur pendant la fermeture du capteur\n");
	}
	
	if(pe_driverUninstall())
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;	
}
/**
 * Test 11 - Ouverture d'un capteur avec des param�tres invalides
 * Description
 * Appeler open sur un capteur valide (le fichier existe, et est accessible en lecture/�criture, en
 * passant un mode diff�rent de O_RDONLY.
 * Resultat attendu
 * .
 * L'appel doit �chouer, et donc renvoyer ERROR. De plus, errno doit �tre positionn� � EARG.
 **/
int test_11()
{
	int success = 0;
	int fd = 0;
	
	pe_driverInstall(1);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDWR, 0);
	if( fd == ERROR )
	{
		if(errnoGet() == EARG)
		{
			success++;
		}
		else
		{
			printf("Mauvais errno");
		}
	}
	
	close(fd);
	
	if(pe_driverUninstall())
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;
}
/**
 * Test 12 - Fermeture d'un capteur
 * Description
 * Appeler close sur un descripteur de fichier valide (qui a �t� ouvert avec succ�s pr�c�demment),
 * et qui n'a pas �t� ferm� depuis.
 * Resultat attendu
 * La valeur de retour doit �tre �gale � OK
 **/
int test_12()
{
	int success = 0;
	int fd = 0;
	
	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDONLY, 0);
	if( fd != ERROR )
	{
		if(close(fd) == ERROR)
		{
			printf("Erreur a la fermeture du capteur\n");
		}
		else
		{
			success++;
		}
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}

	if(pe_driverUninstall())
	{
		printf("Erreur a l'enlevement du driver\n");
	}

	return success;
}
/**
 * Test 13 - Lecture d'une valeur dans un capteur
 * Description
 * Utiliser read sur un capteur ouvert avec succ�s avant puis apr�s avoir
 * envoy� un message
 * Resultat attendu
 * La valeur de retour doit �tre : 
 * - ERROR avec errno sur ENOAVAIL
 * - un nombre positif, et doit �tre coh�rente par rapport aux param�tre
 * d'appel de read.
 **/
int test_13()
{
	int success = 0;
	int fd = 0;
	
	Message buffer;
	char* data = "fake DATA :)\0";
	static int id = -1;
	struct timespec date;
	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur2", O_RDONLY, 0);
	if(fd != ERROR)
	{
		ioctl(fd, SET_CPT_ADDRESS, 42);
		
		if(read(fd,(char*)&(buffer), sizeof(buffer)) < 0)
		{
			if(errnoGet() == ENOAVAIL)
			{
				success++;
			}
			else
			{
				printf("Erreur : mauvais message d'erreur\n");
			}
		}
		else
		{
			printf("Erreur : lecture avec succ�s sur un capteur vide\n");
		}
		
		hardware_mockup(42, data);
		id++;
		clock_gettime(CLOCK_REALTIME, &(date));
		// l'attente est obligatoire sinon le message n'est pas encore trait�.
		sleep(1);
		if(read(fd,(char*)&(buffer), sizeof(buffer)) < 0)
		{
			printf("Erreur a la lecture %d\n", errnoGet());
		}
		else
		{
			if(date.tv_sec != buffer.date.tv_sec)
			{
				printf("Erreur : mauvaise date \n");
			}
			else
			{			
				if(strncmp(buffer.msg, data, MAX_DATA_SIZE) == 0)
				{
					success++;
				}
				else
				{
					//printf("Message:\n- id : %d\n- date : %ld:%ld\n- Message : %s\n", buffer.id, buffer.date.tv_sec, buffer.date.tv_nsec, buffer.msg);
		
					printf("Erreur : le message recus n'est pas le message envoye\n");
					//printf("envoye : %s\n", data);
					//printf("recus : %s\n", buffer.msg);
					
				}
			}
		}
		
		if(close(fd) == ERROR)
		{
			printf("Erreur a la fermeture du capteur\n");
		}
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}


	if(pe_driverUninstall() == ERROR)
	{
		printf("Erreur a l'enlevement du driver\n");
	}

	return (success == 2) ? 1 : 0;
}
/**
 * Test 14 - Utilisation de read avec une taille de lecture invalide
 * Description
 * Utilisation de l'appel syst�me read avec un descripteur de fichier valide, mais avec une taille
 * de lecture n�gative.
 * Resultat attendu
 * L'appel doit �chouer en renvoyant ERROR, et errno doit �tre positionn� � EARG.
 **/
int test_14()
{
	int success = 0;
	int fd = 0;
	Message buffer;
	
	pe_driverInstall(1);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDONLY, 0);
	if(fd != ERROR)
	{
		hardware_mockup(0, "fake DATA :)\0");

		if(read(fd, (char*)&(buffer), -42) == ERROR)
		{
			success++;
		}
		
		if(close(fd) == ERROR)
		{
			printf("Erreur a la fermeture du capteur\n");
		}
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}
		

	if(pe_driverUninstall() == ERROR)
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;		
}
/**
 * Test 15 - Utilisation de ioctl avec des param�tres corrects
 * Description
 * Utilisation de ioctl avec des param�tres corrects : un descripteur de fichier valide, une valeur
 * pour request �gale � la constante SET_CPT_ADDRESS, et une valeur pour value inf�rieur ou
 * �gale � 255, correspondant bien � un capteur valide.
 * Resultat attendu
 * La valeur de retour doit �tre �gale � OK, ou alors elle doit �tre �gale � ERROR, mais alors errno
 * doit �tre positionn� � ECPTBUSY, et le m�me appel effectu� ult�rieurement doit renvoyer OK.
 **/
int test_15()
{
	int success = 0;
	int fd = 0;
	
	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDONLY, 0);
	if(fd != ERROR)
	{
		if(ioctl(fd, SET_CPT_ADDRESS, 42) == OK)
		{
			success++;
		}
		else
		{
			printf("Erreur lors de l'appel a ioctl\n");
		}
		
		if(close(fd) == ERROR)
		{
			printf("Erreur a la fermeture du capteur\n");
		}
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}


	if(pe_driverUninstall() == ERROR)
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;
}
/**
 * Test 16 - Utilisation de ioctl avec de mauvais arguments
 * Description
 * Utilisation de la fonction ioctl avec un second param�tre correspondant � une fonction nonimpl�ment�.
 * Le descripteur de fichier pass� en tant que premier param�tre doit �tre valide, et
 * correspondre � un p�riph�rique g�r� par le driver.
 * Resultat attendu
 * L'appel doit �chouer (la valeur de retour doit �tre �gale � ERROR), et errno doit �tre positionn�
 * � EARG.
 **/
int test_16()
{
	int success = 0;
	int fd = 0;
	
	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDONLY, 0);
	if(fd != ERROR)
	{
		if(ioctl(fd, 42, 42) == ERROR)
		{
			if(errnoGet() == EARG)
			{
				success++;
			}
			else
			{
				printf("Erreur : Mauvais code d'erreur\n");
			}
		}
		else
		{
			printf("Erreur lors de l'appel a ioctl\n");
		}
		
		if(close(fd) == ERROR)
		{
			printf("Erreur a la fermeture du capteur\n");
		}
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}


	if(pe_driverUninstall() == ERROR)
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;
}
/**
 * Test 17 - Utilisation de ioctl pour associer le m�me capteur � deux descripteur
 * de fichiers
 * Description
 * On utilise la fonction ioctl deux fois, avec des descripteurs de fichier diff�rents, mais avec le
 * m�me num�ro de capteur.
 * Resultat attendu
 * L'appel doit �chouer, et renvoyer ERROR. errno doit alors �tre positionn� � ECPTALREADYUSED.
 **/
int test_17()
{
	int success = 0;
	int fd0 = 0;
	int fd1 = 0;
	
	pe_driverInstall(10);
	
	// Ouvertude d'un capteur.
	fd0 = open("/dev/capteur0", O_RDONLY, 0);
	fd1 = open("/dev/capteur1", O_RDONLY, 0);
	if(fd0 != ERROR && fd1 != ERROR)
	{
		if(ioctl(fd0, SET_CPT_ADDRESS, 42) == OK)
		{
			if(ioctl(fd0, SET_CPT_ADDRESS, 42) == ERROR)
			{
				if(errnoGet() == ECPTALREADYUSED)
				{
					success++;
				}
				else
				{
					printf("Erreur : Mauvais code d'erreur\n");
				}
			}
			else
			{
				printf("Erreur : l'appel a ioctl n'a pas echoue, il existe deux capteur pointant vers la m�me adresse CAN\n");
			}
		}
		else
		{
			printf("Erreur lors de l'appel a ioctl sur le premier capteur\n");
		}
		
		if(close(fd0) == ERROR || close(fd1) == ERROR)
		{
			printf("Erreur a la fermeture d'un des capteurs\n");
		}
	}
	else
	{
		printf("Erreur a l'ouverture d'un des capteurs\n");
	}


	if(pe_driverUninstall() == ERROR)
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;
}
/**
 * Test 18 - Utilisation de write
 * Description
 * Appel de write sur un capteur ouvert avec succ�s.
 * Resultat attendu
 * L'appel doit �chouer, et errno doit �tre positionn� � ENSUP.
 **/
int test_18()
{
	int success = 0;
	int fd = 0;
	Message buffer;
	
	pe_driverInstall(1);
	
	// Ouvertude d'un capteur.
	fd = open("/dev/capteur0", O_RDONLY, 0);
	if(fd != ERROR)
	{
		if(write(fd,(char*)&(buffer), sizeof(Message)))
		{
			if(errnoGet() == ENOTSUP)
			{
				success++;
			}
			else
			{
				printf("Mauvais numero d'erreur\n");
			}
		}
		else
		{
			printf("Erreur : lecture avec succes");
		}
		
		if(close(fd) == ERROR)
		{
			printf("Erreur a la fermeture du capteur\n");
		}
	}
	else
	{
		printf("Erreur a l'ouverture du capteur\n");
	}


	if(pe_driverUninstall() == ERROR)
	{
		printf("Erreur a l'enlevement du driver\n");
	}
	
	return success;		
}

/**
 * Test 19 - Retrait d'un p�riph�rique ouvert
 * Description
 * Tentative de retrait d'un p�riph�rique sur lequel un appel de open a �t� 
 * effectu� avec succ�s pr�c�demment, et sur lequel on n'a pas fait d'appel
 * � close ou remove.
 * Resultat attendu
 * L'appel doit r�ussir : la valeur de retour doit �tre �gale � OK.
 **/
int test_19()
{
	int success = 0;
	int fd = 0;
	pe_driverInstall(15);
	fd = creat("/dev/capteur0", O_RDONLY);
	
	if(pe_deviceRemove(0) == OK)
	{
		success++;
	}
	else
	{
		ERR_MSG(19);
	}
	pe_driverUninstall();
	return success;
}
/**
 * Test 20 - Retrait d'un p�riph�rique qui n'existe pas
 * Description
 * Tentative de retrait d'un p�riph�rique qui n'existe pas.
 * Resultat attendu
 * L'appel doit �chouer, et retourner ERROR. errno doit �tre positionn� � 
 * ENEXIST.
 **/
int test_20()
{
	int success = 0;
	int fd = 0;
	pe_driverInstall(15);
	// Le capteur 67 n'existe pas.
	if(pe_deviceRemove(67) == ERROR && errnoGet() == ENEXIST)
	{
		success++;
	}
	else
	{
		ERR_MSG(20);
	}
	pe_driverUninstall();
	return success;
}
void run_suite()
{
	int count = 0;
	test_1() ? count++ : printf("test_1 : ERROR\n");
	test_2() ? count++ : printf("test_2 : ERROR\n");
	test_3() ? count++ : printf("test_3 : ERROR\n");
	test_4() ? count++ : printf("test_4 : ERROR\n");
	test_5() ? count++ : printf("test_5 : ERROR\n");
	test_6() ? count++ : printf("test_6 : ERROR\n");
	test_7() ? count++ : printf("test_7 : ERROR\n");
	test_8() ? count++ : printf("test_8 : ERROR\n");
	test_9() ? count++ : printf("test_9 : ERROR\n");
	test_10() ? count++ : printf("test_10 : ERROR\n");
	test_11() ? count++ : printf("test_11 : ERROR\n");
	test_12() ? count++ : printf("test_12 : ERROR\n");
	test_13() ? count++ : printf("test_13 : ERROR\n");
	test_14() ? count++ : printf("test_14 : ERROR\n");
	test_15() ? count++ : printf("test_15 : ERROR\n");
	test_16() ? count++ : printf("test_16 : ERROR\n");
	test_17() ? count++ : printf("test_17 : ERROR\n");
	test_18() ? count++ : printf("test_18 : ERROR\n");
	test_19() ? count++ : printf("test_19 : ERROR\n");
	test_20() ? count++ : printf("test_20 : ERROR\n");
	

	if(count == 20)
	{
		printf("All tests passed successfully.\n");
	}
	else
	{
		printf("%d/20 tests passed.\n", count);
	}
}

