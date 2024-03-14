/*
 * Lab6completo.c
 *
 *  Created on: 16 gen 2024
 *      Author: Utente
 */

//Super Car

#include "xstatus.h"
#include "xtmrctr_l.h"
#include "xil_printf.h"
#include "xparameters.h"

#ifndef SDT
#define TMRCTR_BASEADDR		XPAR_TMRCTR_0_BASEADDR
#else
#define TMRCTR_BASEADDR		XPAR_XTMRCTR_0_BASEADDR
#endif

#define TIMER_COUNTER_0	 0

int flag=3, led,counter=1;
void myISR(void)__attribute__((interrupt_handler));
int TimerCounter(UINTPTR TmrCtrBaseAddress, u8 TimerCounter);

void myISR(){

	u32 control_status;
	switch(flag){//Flag mi dice quale bottone ho premuto
		case 0: //Bottone destro
			if(led)
				led=led>>1;//Shifto di una posizione a destra
			else //se led=0
				led=0x80;//Riporto alla codizione di partenza
			break;
		case 1://Bottone sinistro
			if(led==0x8000)//Se è accesso l'ultimo bit a sinistra
				led=0x100;//riporto alla condizione di partenza
			else//altrimenti shifto di una posizione vestro sinistra
				led=led<<1;
			break;
		case 2://Se non ho premuto ne destra ne sinistra resto nella condizione precedente
			led=led;
			break;
	}
	Xil_Out32(XPAR_AXI_16LEDS_GPIO_BASEADDR,led);

	//Ack timer, setto a 0 il bit 8 (T0INT)  del TCSR
	//leggo lo stato del registro TCSR
	control_status=*((unsigned int*)XPAR_AXI_TIMER_0_BASEADDR);
	//modifico solo il bit 8
	*((unsigned int*)XPAR_AXI_TIMER_0_BASEADDR)=control_status | 0x0100;
	/*ack INTC- base_address+0x0C (IAR)
   * Questo registro serve per pulire le richieste dell'interrup dopo che la periferica
   * è stata servita settando a 1 il canale dal quale è arrivato l'interrupt.
   */
	*((unsigned int*)(XPAR_AXI_INTC_0_BASEADDR+0xC))=0x4;

}


int TimerCounter(UINTPTR TmrCtrBaseAddress, u8 TmrCtrNumber){
	u32 ControlStatus;

	//Pulisco il valore del Control Status Register
	XTmrCtr_SetControlStatusReg(TmrCtrBaseAddress, TmrCtrNumber, 0x0);
	//Accediamo a load register e ci carichiamo dentro il volore al quale vogliamo far
	//arrivare il conteggio, in questo caso vogliamo che conti fino 3000000
	XTmrCtr_SetLoadReg(TmrCtrBaseAddress, TmrCtrNumber,  3000000 );
	// Trasferiamo il contenuto del Load register nel counter settando a 1 il bit 5
	//del TCSR.
	XTmrCtr_LoadTimerCounterReg(TmrCtrBaseAddress, TmrCtrNumber);

	//Rileggiamo il valore contenuto nel TCST per settare i diversi bit che mi abilitino
	//il conteggio

	//a 0 il bit del Load register
	//(bit 5) così da abilitare il conteggio
	ControlStatus = XTmrCtr_GetControlStatusReg(TmrCtrBaseAddress,
			TmrCtrNumber);

	/*Dopo aver letto il contenuto del TCSR setto a  0 il bit del Load register
	*(bit 5) così da abilitare il conteggio e successivamente con l'OR  setto a 1
	*i bit 1,4 e 6 rispettivamente per abilitare il down counter, l'autoreload e
	*l'interrupt
	*/
	XTmrCtr_SetControlStatusReg(TmrCtrBaseAddress, TmrCtrNumber,
				    (ControlStatus & (~XTC_CSR_LOAD_MASK))|(0x52));

   /*A questo punto inizia il conteggio andando a decrementare il valore caricato,
    * quando il conteggio arriva a 0 il timer genererà un interrupt e il conteggio
    * ripartirà dal valore caricato grazie all'autoreload
    */

	//faccio partire il conteggio
	XTmrCtr_Enable(TmrCtrBaseAddress, TmrCtrNumber);//il bit 7 viene settato a 1 e inizia il conteggio

}


typedef enum {pressed, idle} debounce_state_t;
int FSM_debounce(int buttons){
	static int debounced_buttons;
	static debounce_state_t currentState = idle;
	switch (currentState) {
		case idle:
			debounced_buttons=buttons;
			// Fin quando non premo un bottone sto nella condizione di idle, se no passo a pressed
			if (buttons!=0)
			currentState=pressed;
			break;
		case pressed:
			debounced_buttons=0;
			if (buttons==0)//Appena mollo il bottone ritorno alla condizione di idle
			currentState=idle;
			break;
	}
	return debounced_buttons;
}


typedef enum {center, right, left } state_t;

int My_FSM(int buttons){
	static int state_buttons;
	static state_t currentState = center;
	switch (currentState) {
		case center:
			if(buttons==8){//Se sono in center e ho premuto il bottone di destra passo allo stato right
				currentState=right;
				state_buttons=1;
			}
			else if(buttons==2){//Se sono in center e ho premuto il bottone di sinistra passo allo stato left
				currentState=left;
				state_buttons=2;
			}
			else{//Se ho premuto di nuovo il bottone centra o quello alto/basso continuo a stare nello stato center
				state_buttons=0;
				currentState=center;
			}

			break;
		case right:

			if(buttons==2){//Se sono in right e ho premuto il bottone di sinistra passo allo stato left
				state_buttons=2;
			}
			else if(buttons==8){//Se sono in right e ho premuto il bottone di destra rimango nello stato right
				state_buttons=1;
				currentState=right;
			}
			else{//Se sono in right e ho premuto il bottone di centrale/alto/basso passo allo stato center
				state_buttons=0;
				currentState=center;
			}
			break;
		case left:
			if(buttons==8){//Se sono in left e ho premuto il bottone di destra passo allo stato right
				currentState=right;
				state_buttons=1;
			}
			else if(buttons==2){//Se sono in left e ho premuto il bottone di sinistra rimango nello stato left
				state_buttons=2;
				currentState=left;
			}
			else{ //Se sono in left e ho premuto il bottone centrale/alto/basso passo allo stato center
				state_buttons=0;
				currentState=center;
			}

			break;
	}
	//state_buttons restituisce un valore pari a 0 se siamo nello stato center,
	//1 se siamo in right e 2 se siamo in left
	return state_buttons;
}

void state_evaluation(int new_state, int last_state){

	//Se il vecchio stato è diverso dal nuovo o counter è un numero pari
	if(new_state!=last_state | counter % 2 == 0){
		switch(new_state){
			case 0:
				//Se ho premuto il bottone centrale non faccio nulla
				xil_printf("center\r\n");
				break;
			case 1:
				xil_printf("right\r\n");
				//Se ho premuto il bottone di destra accendo l'ottavo led da destra
				led=0x80;
				flag=0;//Mi serve nell'interrup per capire quale bottone è stato premuto
				break;

			case 2:
				xil_printf("left\r\n");
				//Se ho premuto il bottone a sinistra accendo il led più significativo
				led=0x100;
				flag=1;//Mi serve nell'interrup per capire quale bottone è stato premuto
				break;

		}
		if(counter % 2 == 0)
			/*Quando clicco una volta un bottone (dx,sx) lo stato precedente e quello successivo
			 * sono diversi, quindi entro nell'if e accendo il led, la seconda volta che premo lo
			 * stesso bottone entro nell'else e counter diventa pari. Se riclicco, lo stato precedente
			 * e quello successivo saranno sempre uguali ma, essendo counter pari entrerò comune nell'if.
			 * A questo punto devo riportare counter a un numero dispari così che ricliccando lo stesso
			 * bottone e avendo stati uguali e counter dispari io possa entrare nell'else.
			 */
			counter++;
	}
	else if(new_state!=0){//Se invece lo stato precedente è uguale al nuovo stato e counter è dispari entro nell'else
		//Setto led a 0 per poterli spegnere
		led=0;
		flag=2;//Mi serve nell'interrup per capire quale bottone è stato premuto
		//incremento counter
		counter++;

	}
}



int main(void)
{
	init_platform();
	int buttons,status,new_state=0, last_state=0;
	//enable INTC
	/* enable MER, base_addres+0x1C
   *E' usato per abilitare le richieste di interrupts verso il processore, i due bit meno
   *significativi vengono settati a 1 per abilitare le richieste delle le periferiche.
   */
	*((unsigned int*)(XPAR_AXI_INTC_0_BASEADDR+0x1C))=0x3;
	/*
   * enable  IER, base_addres+0x08
   * E' usato per abilitare la selezione degli interruputs.
   * Setto 1 il 3 bit meno significativo e gli altri a 0 perchè l'interrupt del timer arrriva
   * dal canale 3.
   */
	*((unsigned int*)(XPAR_AXI_INTC_0_BASEADDR+0x08))=0x4;
	microblaze_enable_interrupts ();
	//Faccio partire il timer
	TimerCounter(TMRCTR_BASEADDR, TIMER_COUNTER_0);

	while(1){
		//Metto dentro buttons lo stato dei bottoni
		buttons=Xil_In32(XPAR_AXI_BUTTONS_GPIO_BASEADDR);
		//Se status!=0 allora vuol dire che ho premuto i bottoni e sono passata da uno stato idle a uno stato pressed
		status=FSM_debounce(buttons);

		if(status!=0){//Se ho premuto il bottone
			//Imposto lo stato precedente a quello letto all'iterazione precedente
			last_state=new_state;
			//Aggiorno lo stato attuale (sarà 0,1 o 2 a seconda del bottone premuto)
			new_state=My_FSM(buttons);
			state_evaluation(new_state,last_state);

		}
	}

	cleanup_platform();
}


