/*
 * main.h
 *
 *  Created on: 2020. 6. 12.
 *      Author: gauib
 */

#ifndef MAIN_H_
#define MAIN_H_

#define DCMOTER_BB_PIN			(6U)
#define DCMOTER_BB_PORT			(gpioPortC)
#define DCMOTER_BA_PIN			(7U)
#define DCMOTER_BA_PORT			(gpioPortC)
#define DCMOTER_AA_PIN			(1U)
#define DCMOTER_AA_PORT			(gpioPortB)
#define DCMOTER_AB_PIN			(0U)
#define DCMOTER_AB_PORT			(gpioPortB)

void Set_Left_Speed(int speed);
void Set_Right_Speed(int speed);

void Init_Timer(void);

#endif /* MAIN_H_ */
