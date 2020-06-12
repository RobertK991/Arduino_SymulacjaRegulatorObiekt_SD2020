#include <stdio.h>
#include <math.h>
#include <time.h>

float TaskObiekt(float sygnalSterujacy);						//Funkcja obiektu
float TaskRegulator(float wartoscBiezaca, int wartosc_zadana);	//Funkcja regulatora
void sleep_us(unsigned long microseconds);  		//Funkcja spania
void delay(int number_of_seconds);			//Funkcja opóźnienia
//Struktura parametrów regulatora
struct parametr
{
	float KP;
	float TI;
	float TD;
	float WZ;
};

int main ()
{
	printf ("Symulacja obiektu i regulatora\n");
	float sygnalSterujacy = 1;      //Wasrtosc sygnalu steruajcego
	float wartoscBiezaca = 1;                //Wartosc bieżąca sygnału wyjścia z obiektu
	clock_t t;				///Zmienna zegara
	time_t rawtime;			//Zmienna czasu
	struct tm * timeinfo;	//Zmienna do odczytu czasu
	int wartosc_zadana;		//Zmienna przechowująca informacje o wartości zadanej
	int input;		//Zmienna do odczytu danych z konsoli
	printf("Podaj wartość zadaną z przedziału 0 - 99 \n");		///Komunikat o wartości zadanej
	scanf("%d", &input);		//Odczyt danych
	getchar();					//Zaczekanie na dane
	wartosc_zadana = input;		//Przepisanie wartości zadanej
	//Obliczenie wartości zadanej
	while(1)
	{
		//Sprawdzenie czy należy z przedziału 0 - 99
		if (wartosc_zadana<=99 && wartosc_zadana >= 0 )
		{
			printf("Wartość zadana wynosi: %i. \n", wartosc_zadana);
			break;
		}
		//Jeśli nie to pobranie jej ponownie
		else
		{
			printf("Ooppps coś poszło nie tak,\nwartośc zadana musi mieścić się w przedziale 0 - 99.\nSpróbuj jeszcze raz:\n");
			scanf("%d", &input);
			wartosc_zadana = input;
			getchar();
		}
	}
	//Pętla główna
	while(1)
	{
		t = clock();		//Odczyt ticków zegara startu
		sygnalSterujacy = TaskRegulator(wartoscBiezaca, wartosc_zadana);	//Funkcja regulatora
		wartoscBiezaca = TaskObiekt(sygnalSterujacy);						//Funkcja obiektu
		//sleep_us(100000);
		delay(100);			//Opóźnienie o 100ms
		time ( &rawtime );	//Odczyt czasu
  		timeinfo = localtime ( &rawtime );	//Przerobienie czasu na foramt do odczytu	
  		printf ( "Data: %s", asctime (timeinfo) );
		t = clock() - t;	//Odczyt ticków zegara stopu i obliczenie różnicy
		printf ("Zajęło %ld clicks (%f sekund).\n",t,((float)t)/CLOCKS_PER_SEC);
	}
	return 0;
}
/*
-----------------------Funkcja czekania w mikro sekundach---------------
*/
void sleep_us(unsigned long microseconds)
{
    struct timespec ts;
    ts.tv_sec = microseconds / 1e6;             // whole seconds
    ts.tv_nsec = (microseconds % 1000000) * 1e3;    // remainder, in nanoseconds
    nanosleep(&ts, NULL);
}
/*
-----------------------Funkcja delay------------------------------------
*/
void delay(int number_of_seconds) 
{ 
    int milli_seconds = 1000 * number_of_seconds; //Sekundy na milisekundy
    clock_t start_time = clock(); 					//Czas startu
    while (clock() < start_time + milli_seconds) 	//Nic nie robi - czeka
        ; 
} 
/*
-----------------------Funkcja regulatora--------------------------------
*/
float TaskRegulator(float wartoscBiezaca, int wartosc_zadana)
{
	float up = 0, ui = 0, ud = 0, alfa = 0.3;    	//Zmienne poszczególnych członów
	float  roznica = 0, przyrost_ud = 0;    		//Zmienne do części różniczkowej
	float suma = 0, sumaStara = 0;              	//Zmienne do częsci całkowej 
	float h = 0.1;           						//Okres impulsowania
	float uchyb = 0, uchybStary = 0, udStare = 0;	//Stary uchyb i bieżący;
	float umax=5000;            //Zmienna do anty-windup
	struct parametr parametry;
	//Parametry poczatkowe
	parametry.KP = 0.1;
	parametry.TD = 1;
	parametry.TI = 4;
	parametry.WZ = wartosc_zadana;
	float sygnalSterujacy = 0;
	uchyb=parametry.WZ - wartoscBiezaca;    	//Wartośc uchybu          
	roznica=(uchyb - uchybStary)/h;				//Różnica do członu D
	przyrost_ud=(-1/(alfa*parametry.TD))*udStare+(parametry.TD/(alfa*parametry.TD) )*roznica;	//Przyrost członu D
	suma=uchyb+sumaStara;		//Suma do I
	//Poszczególne parametry regulatora
	up=parametry.KP*uchyb;
	ui=(h/parametry.TI)*suma;
	ud=udStare+h*przyrost_ud;
	//Sygnał sterujący
	sygnalSterujacy = up+ui+ud;       
	//Stare wartości
	uchybStary = uchyb;
	sumaStara = suma;
	udStare = ud;                                  //Wypisuje wartość
	// //anty-windup
	if (sygnalSterujacy<=0) { sygnalSterujacy=0;  suma=uchyb;}
	if (sygnalSterujacy>umax) {sygnalSterujacy=umax; suma-=uchyb;}
	//Wypisanie na konsoli
	printf("Sygnał sterujący: ");
	printf("%f", sygnalSterujacy);
	printf(" Uchyb: ");
	printf("%f", uchyb);
	//printf(" Czas: %ld", time(NULL));
	return sygnalSterujacy;
	}
//Zmienne obiektu
float H1 = 0, H2 = 0, h1 = 0, h2 = 0;
/*
-----------------------Funkcja Obiektu--------------------------------------
*/
float TaskObiekt(float sygnalSterujacy)
{
	float h = 0.1;
	float wartoscBiezaca = 0;
	float K1 = 1, K2 = 2;
	float T1 = 4, T2 = 7;
	// float dt = 0.001;
	// float u0 = 10;
	// float x1 = 0, x10 = 0;
	// int licznik = round(h/dt);
	// float Y;
	// Transmitancja
	//          K
	// G(s)= --------
	//        1 + Ts
	// float K = 2;
	// float T = 3;
	// Transmitancja
	//              K1 * K2
	// G(s)= -------------------------
	//        (1 + T1*s)*(1 + T2*s)
	//Parametry biorników
	// float k11,k21;
	// float A1 = 14,A2 = 100;            //Powierznia przekroju zbiornika 1 i 2 
	// float sn1 = 12;         //Przekrój przepływowy zaworu 1
	// float sn2 = 30;         //Przekrój przepływowy zaworu 2
	// float hn1 = 30;         //Poziom wody w punkcie pracy
	// float hn2 = 60;         //Poziom wody w punkcie pracy
	// float g = 10;           //Przyśpieszenie ziemskie
	// k11 = sn1*sqrt(g/(2*hn1));
	// k21 = sn2*sqrt(g/(2*hn2));
	// K1 = 1/k11;             //Wzmocnienie 1 zbiornika
	// T1 = A1/k11;            //Stała czasowa 1 zbiornika
	// K2= 1/k21;             //Wzmocnienie 2 zbiornika
	// T2 = A2/k21;            //Stała czasowa 2 zbiornika

	// //Obiekt 1
	// H1 = exp(-h/T1)*h1+K1*(1-exp(-h/T1))*sygnalSterujacy;
	// h1 = H1;
	// wartoscBiezaca = H1;
	//Obiekt 2
	//H1 = expf(-h/T1)*h1+K1*(1-expf(-h/T1))*sygnalSterujacy;
	//H2 = expf(-h/T2)*h2+K2*(1-expf(-h/T2))*h1;
	H1 = expf(-0.1/40)*h1+K1*(1-expf(-0.1/40))*sygnalSterujacy;
	H2 = expf(-0.1/70)*h2+K2*(1-expf(-0.1/70))*h1;
	h1 = H1;
	h2 = H2;
	wartoscBiezaca = H2;		//Wartość bieżąca
	//Ograniczenia
	if (wartoscBiezaca <= 0) { wartoscBiezaca = 0;}
	if (wartoscBiezaca >= 100) { wartoscBiezaca = 100;}
	//Wypisanie w konsoli
	printf("  Wartość bieżąca: ");
	printf("%f \n", h2);
	return wartoscBiezaca;
}
