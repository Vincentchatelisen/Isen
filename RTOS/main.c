/*
 * TP Centre de tri de colis - version étudiant
 * 4/5/2019 - YA
 */
 
// includes Kernel
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// includes Standard 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// includes Simulateur (redirection de la sortie printf)
#include "simulateur.h"

// Définition du nombre d'éléments pour les files
#define x 10 // nombre d'éléments pour la File_tapis_arrivee 
#define y 10 // nombre d'éléments pour la File_depart_national et File_depart_international
#define z 10 // nombre d'éléments pour la File_tapis_relecture 

// Définition du delai pour la tâche de relecture
#define DELAI_RELECTURE   500

// Définition des timeout pour les files en ms
#define TIMEOUT_FILE_TAPIS_ARRIVEE   100
#define TIMEOUT_FILE_TAPIS_RELECTURE 100
#define TIMEOUT_FILE_TAPIS_DEPART		 100  // Timeout identique en national ou international

// Création des files
xQueueHandle File_tapis_arrivee;
xQueueHandle File_depart_national;
xQueueHandle File_depart_international;
xQueueHandle File_tapis_relecture;

// Création du sémaphore pour la ressource partagée
xSemaphoreHandle xSemaphore;


//********************************************************
//* Tâche arrivée 
//*
//* Répète de façon cyclique une liste de colis à deposer
//*
//* B2=1 -> le colis est passé par « tache-relecture », sinon B2=0
//* B1=1 -> étiquette non lisible sinon B1=0
//* B0=1 -> marché international / B0=0 -> marché international
//* 
//********************************************************
void tache_arrivee( void *pvParameters )
{	
	unsigned int liste_colis[]={ 1,  3,  1,  2,  3,  0}; // Etat des bits B2, B1 et B0 -> 0 à 3 en décimal car B2=0
	unsigned int liste_delai[]={ 5,100,200, 30,400, 50}; // Temps d'attente en ms pour le colis suivant
	unsigned int num_colis = 0; // Initilisation du numéro de colis
	unsigned int colis; // Colis (étiquette)
	while(1)
	{ 
		colis = (num_colis<<3) + liste_colis[num_colis % (sizeof(liste_colis)/sizeof(unsigned int))];
		xQueueSendToBack(File_tapis_arrivee, &colis, TIMEOUT_FILE_TAPIS_ARRIVEE); // Il faudra gérer le débordement et afficher un message d'erreur
		vTaskDelay(liste_delai[num_colis % (sizeof(liste_colis)/sizeof(unsigned int))]/portTICK_RATE_MS); // Attente entre deux colis en ms
		if(xSemaphore != NULL){
			if(xSemaphoreTake(xSemaphore,portMAX_DELAY == pdTRUE)){
				printf("Le colis No %d est depose sur le tapis roulant et il porte l'etiquette %d\n",num_colis, colis);	// Il faudra gérer l'affichage comme une ressource partagée
				xSemaphoreGive(xSemaphore);
			}
		}	
		num_colis++;
		
  }
	vTaskDelete( NULL );
}

void affiche_message(char *texte, unsigned int colis){
	if(xSemaphoreTake(xSemaphore,portMAX_DELAY == pdTRUE)){
		printf("%s : compteur= %d B2= %d B1= %d B0= %d",texte ,colis,(colis & (1<<2)>>2),(colis & (1<<1)>>1),(colis & (1<<0))  );
		xSemaphoreGive(xSemaphore);
	}
	
}

void tache_lecture_rapide(void *pvParameters){
	unsigned int colis;
	//vTaskDelay(1000);
	while(1)
		{
		if(File_tapis_arrivee !=0){
			xQueueReceive(File_tapis_arrivee, &colis,TIMEOUT_FILE_TAPIS_ARRIVEE);
			if((colis & (1<<1)>>1) == 1){
				xQueueSendToBack(File_tapis_relecture, &colis, TIMEOUT_FILE_TAPIS_RELECTURE); // Il faudra gérer le débordement et afficher un message d'erreur
			}
			else{
				if((colis & (1<<0)>>0) == 1){
					xQueueSendToBack(File_depart_international, &colis, TIMEOUT_FILE_TAPIS_DEPART); // Il faudra gérer le débordement et afficher un message d'erreur
				}
				else{
					xQueueSendToBack(File_depart_national, &colis, TIMEOUT_FILE_TAPIS_DEPART); // Il faudra gérer le débordement et afficher un message d'erreur
				}
			}
		}
	}
	vTaskDelete( NULL );
}

void tache_depart_national(void *pvParameters){
	unsigned int colis;
	while(1)
		{
		if(File_depart_national != 0){
			xQueueReceive(File_depart_national, &colis,TIMEOUT_FILE_TAPIS_DEPART);
			affiche_message("Depart national",colis);
		}
	}
	vTaskDelete( NULL );
}
void tache_depart_international(void *pvParameters){
	unsigned int colis;
	//vTaskDelay(1000);
	while(1)
		{
		if(File_depart_international != 0){
			xQueueReceive(File_depart_international, &colis,TIMEOUT_FILE_TAPIS_DEPART);
			affiche_message("Depart international",colis);
		}
	}
	vTaskDelete( NULL );
}

// Main()
int main(void)
{
	// Variable d'état du code de retour (non utilisé pour simplifier)
	portBASE_TYPE xReturned;
	// ulTaskNumber[0] = tache_arrivee
	// ulTaskNumber[1] = idle task
	
	// Création des files
	File_tapis_arrivee  			= xQueueCreate(x, sizeof(unsigned int));
	File_depart_national = xQueueCreate(y, sizeof(unsigned int));
	File_depart_international = xQueueCreate(y, sizeof(unsigned int));
	File_tapis_relecture = xQueueCreate(z, sizeof(unsigned int));
	// Création du sémaphore pour la ressource partagée
	vSemaphoreCreateBinary(xSemaphore);
	// Création de la tâche arrivée
	xReturned = xTaskCreate( 	tache_arrivee,// Pointeur vers la fonction
								"Tache arrivee",					// Nom de la tâche, facilite le debug
								configMINIMAL_STACK_SIZE, // Taille de pile (mots)
								NULL, 										// Pas de paramètres pour la tâche
								1, 												// Niveau de priorité 1 pour la tâche (0 étant la plus faible) 
								NULL ); 									// Pas d'utilisation du task handle
										// Pas d'utilisation du task handle
	xReturned = xTaskCreate( 	tache_lecture_rapide,// Pointeur vers la fonction
								"Tache relecture",					// Nom de la tâche, facilite le debug
								configMINIMAL_STACK_SIZE, // Taille de pile (mots)
								NULL, 										// Pas de paramètres pour la tâche
								1, 												// Niveau de priorité 1 pour la tâche (0 étant la plus faible) 
								NULL ); 									// Pas d'utilisation du task handle
	xReturned = xTaskCreate( 	tache_depart_national,// Pointeur vers la fonction
								"Tache depart national",					// Nom de la tâche, facilite le debug
								configMINIMAL_STACK_SIZE, // Taille de pile (mots)
								NULL, 										// Pas de paramètres pour la tâche
								1, 												// Niveau de priorité 1 pour la tâche (0 étant la plus faible) 
								NULL );
	xReturned = xTaskCreate( 	tache_depart_international,// Pointeur vers la fonction
								"Tache depart international",					// Nom de la tâche, facilite le debug
								configMINIMAL_STACK_SIZE, // Taille de pile (mots)
								NULL, 										// Pas de paramètres pour la tâche
								1, 												// Niveau de priorité 1 pour la tâche (0 étant la plus faible) 
								NULL ); 	 								
										
										
	// Lance le scheduler et les taches associées 
	vTaskStartScheduler();
	// On n'arrive jamais ici sauf en cas de problèmes graves: pb allocation mémoire, débordement de pile par ex.
	while(1);
}

