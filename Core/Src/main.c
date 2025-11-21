/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "spi.h"
#include "GFX_BW.h"
#include "OLED_SSD1306.h"

#include "fonts/fonts.h"
#include <stdlib.h>  // dla rand()
#include <stdio.h>   // dla sprintf()
#include <string.h>
#include <math.h>


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define GPS_BUFFER_SIZE 128
#define MAX_CHARS_PER_LINE 20
#define MAX_LINES 6


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
//SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char gps_buffer[GPS_BUFFER_SIZE];
// BUFORY NA RAMKI GPS
char gga_buffer[GPS_BUFFER_SIZE]; // Bufor na ramkę $GPGGA
char rmc_buffer[GPS_BUFFER_SIZE]; // Bufor na ramkę $GPRMC

volatile uint16_t gps_idx = 0;       // indeks do bufora GPS
volatile uint8_t gps_line_ready = 0; // flaga gotowej linii
uint8_t rx_byte;                     // bufor odbioru jednego znaku

// Zmienne GPS
float latitude = 0.0f;
float longitude = 0.0f;
char ns = 'N';
char ew = 'E';
int satellites = 0;
char time_field[11] = {0};
float altitude = 0.0f;

// Zmienne z ramki RMC
char date_field[7] = {0};     // DDMMYY
float speed_knots = 0.0f;
float speed_kmh = 0.0f;
int fix_valid = 0;            // A = 1, V = 0

char last_lines[MAX_LINES][GPS_BUFFER_SIZE] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_SPI1_Init(void);
void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

///Funkcja dzieląca tekst na linie o maksymalnej długości MAX_CHARS_PER_LINE mieszczacej sie na wyswietlaczu
void SplitTextSimple(const char *input, char lines[MAX_LINES][MAX_CHARS_PER_LINE+1])
{
    int start = 0; //indeks początkowy w tekście do kopiowania
    for(int i = 0; i < MAX_LINES; i++) { //iteracja po wszystkich liniach
        int j;
        //kopiujemy znaki do bieżącej linii
        //max znaków (MAX_CHARS_PER_LINE) lub do końca stringu
        for(j = 0; j < MAX_CHARS_PER_LINE && input[start+j] != '\0'; j++) {
            lines[i][j] = input[start+j];
        }
        lines[i][j] = '\0'; //dodanie znaku końca stringu
        start += MAX_CHARS_PER_LINE; //przesunięcie indeksu startowego do kolejnego fragmentu tekstu
    }
}

//Konwersja wspolrzędnych z formatu NMEA (ddmm.mmmm) na decimal
float convertNMEAToDecimal(const char* nmea)
{
    float val = atof(nmea);          //konwersja stringa na liczbę zmiennoprzecinkową
    int degrees = (float)((int)(val / 100.0f));  //częsc calkowita to stopnie
    float minutes = val - (degrees * 100.0f); //reszta to minuty
    return degrees + minutes / 60.0f;  //zamiana minut na stopnie dziesiętne i zwrocenie wartosci
}

//Parsowanie linii GGA z GPS na zmienne globalne
void parseGGA(const char* line)
{
    char copy[GPS_BUFFER_SIZE];
    strcpy(copy, line); //kopiujemy linie, bo strtok modyfikuje string

    char *token;
    int field_idx = 0;

    //dzielimy linie po przecinkach
    token = strtok(copy, ",");
    while(token != NULL) {
        switch(field_idx) {
            case 1: //pole czasu (hhmmss.sss)
                strncpy(time_field, token, sizeof(time_field)-1);
                time_field[sizeof(time_field)-1] = '\0';
                break;
            case 2: //szerokosc geo
                latitude = convertNMEAToDecimal(token);
                break;
            case 3: //N/S
                ns = token[0];
                if(ns == 'S') latitude = -latitude; //jesli poludnie, wartosc ujemna
                break;
            case 4: //dlugosc geo
                longitude = convertNMEAToDecimal(token);
                break;
            case 5: //E/W
                ew = token[0];
                if(ew == 'W') longitude = -longitude; //jesli zachd, wartosc ujemna
                break;
            case 7: // liczba satelitow
                satellites = atoi(token); //string -> int
                break;
            case 9: //wysokosc NPM
                altitude = atof(token); //string -> float
            default:
                break; //inne pola ignorujemy
        }
        field_idx++;
        token = strtok(NULL, ","); //przechodzimy do nastepnego pola
    }
}

//Parsowanie ramki RMC (czas, data, prędkosc)
void parseRMC(const char* line)
{
    char copy[GPS_BUFFER_SIZE];
    strcpy(copy, line);

    char *token = strtok(copy, ",");
    int field_idx = 0;

    while(token != NULL)
    {
        switch(field_idx)
        {
            case 1: //czas
                strncpy(time_field, token, sizeof(time_field)-1);
                break;

            case 2: //status A/V
                fix_valid = (token[0] == 'A');
                break;

            case 7: //predkosc w wezlach
                speed_knots = atof(token);
                speed_kmh = speed_knots * 1.852f;
                break;

            case 9: //data DDMMYY
                strncpy(date_field, token, sizeof(date_field)-1);
                break;
        }

        token = strtok(NULL, ",");
        field_idx++;
    }
}

void addNewLine(const char* line) {
    //przesuwamy stare linie w dol
    for(int i = MAX_LINES-1; i > 0; i--) {
        strncpy(last_lines[i], last_lines[i-1], GPS_BUFFER_SIZE-1);
        last_lines[i][GPS_BUFFER_SIZE-1] = 0;
    }
    strncpy(last_lines[0], line, GPS_BUFFER_SIZE-1);
    last_lines[0][GPS_BUFFER_SIZE-1] = 0;
}

///Funkcja dzielaca tekst na linie o maksymalnej dlugosci MAX_CHARS_PER_LINE
///Wypelnia tablicę 'lines' rozpoczynajac od podanego 'start_line_index'
void SplitTextWithIndex(const char *input, char lines[MAX_LINES][MAX_CHARS_PER_LINE+1], int start_line_index)
{
    int start = 0; // indeks poczatkowy w tekście do kopiowania
    for(int i = start_line_index; i < MAX_LINES; i++) { //iteracja po liniach, zaczynamy od 'start_line_index'
        int j;

        //Jeśli tekst wejściowy się skończył, wychodzimy
        if (input[start] == '\0') break;

        //kopiujemy znaki do bieżącej linii
        for(j = 0; j < MAX_CHARS_PER_LINE && input[start+j] != '\0'; j++) {
            lines[i][j] = input[start+j];
        }
        lines[i][j] = '\0'; //dodanie znaku końca stringu

        start += MAX_CHARS_PER_LINE; //przesunięcie indeksu startowego do kolejnego fragmentu tekstu
    }
}

//Funkcja do formatowania i wyświetlania danych z globalnych zmiennych
void formatAndDisplayData(char lines[MAX_LINES][MAX_CHARS_PER_LINE+1])
{
    char formatted_time[9]; //hh:mm:ss + '\0'
    char formatted_date[9]; //DD/MM/YY + '\0'

    //FORMATOWANIE CZASU
    if (strlen(time_field) >= 6) {
        strncpy(formatted_time, time_field, 2);
        formatted_time[2] = ':';
        strncpy(formatted_time + 3, time_field + 2, 2);
        formatted_time[5] = ':';
        strncpy(formatted_time + 6, time_field + 4, 2);
        formatted_time[8] = '\0';
    } else {
        strcpy(formatted_time, "--:--:--");
    }

    //FORMATOWANIE DATY
    if (strlen(date_field) >= 6) {
        strncpy(formatted_date, date_field, 2);
        formatted_date[2] = '/';
        strncpy(formatted_date + 3, date_field + 2, 2);
        formatted_date[5] = '/';
        strncpy(formatted_date + 6, date_field + 4, 2);
        formatted_date[8] = '\0';
    } else {
        strcpy(formatted_date, "--/--/--");
    }


    //Clear wszystkie 6 linii (indeksy 0-5)
    for (int i = 0; i < MAX_LINES; i++) {
        memset(lines[i], 0, MAX_CHARS_PER_LINE+1);
    }

    //linia 0 - data i status
    char *fix_status = fix_valid ? "(A)" : "(V)";
    sprintf(lines[0], "Date: %s | %s", formatted_date, fix_status);

    //linia 1 - czas
    sprintf(lines[1], "Time: %s", formatted_time);

    //linia 2 - opcjonalna ilosc satelitow i predkosc
    //sprintf(lines[2], "Sat: %d Spd: %.1f km/h", satellites, speed_kmh);

    //jesli mamy dane to:
    if (satellites > 0) {
        //linia 3 - szerokosc geo
        sprintf(lines[3], "Lat: %.3f %c", fabsf(latitude), ns);

        //linia 4 - dlugosc geo
        sprintf(lines[4], "Lon: %.3f %c", fabsf(longitude), ew);

        //linia 5 - wysokosc NPM
        sprintf(lines[5], "Alt: %.1f m", altitude);

    } else {
        //jesli nie ma danych to czekamy...
        sprintf(lines[3], "Waiting for data...");
    }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  MX_GPIO_Init();
  MX_SPI1_Init();

  //RESET fizyczny
  HAL_GPIO_WritePin(SSD1306_RESET_GPIO_Port, SSD1306_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(50);
  HAL_GPIO_WritePin(SSD1306_RESET_GPIO_Port, SSD1306_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(50);

  //Inicjalizacja OLED
  SSD1306_SpiInit(&hspi1);
  SSD1306_Init();
  SSD1306_Clear(BLACK);
  SSD1306_Display();

  //Start odbioru UART w trybie przerwań
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);

  char lines[MAX_LINES][MAX_CHARS_PER_LINE+1];
  memset(lines, 0, sizeof(lines));

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  SSD1306_Clear(BLACK);
	  GFX_SetFont(font_8x5);

	      //jesli przyszła nowa ramka -> parsuj i aktualizuj dane
	      if (gps_line_ready) {
	          gps_line_ready = 0;

	          if (strncmp(gps_buffer, "$GPGGA", 6) == 0) {
	              strncpy(gga_buffer, gps_buffer, GPS_BUFFER_SIZE - 1);
	              gga_buffer[GPS_BUFFER_SIZE - 1] = '\0';
	              parseGGA(gga_buffer); //aktualizacja czasu, satelity, szer/dlu geo

	          }
	          else if (strncmp(gps_buffer, "$GPRMC", 6) == 0) {
	              strncpy(rmc_buffer, gps_buffer, GPS_BUFFER_SIZE - 1);
	              rmc_buffer[GPS_BUFFER_SIZE - 1] = '\0';
	              parseRMC(rmc_buffer); //aktualizacja fix i predkosci
	          }
	      }

	      //formatowanie czytelnych danych na górne linie
	      formatAndDisplayData(lines);

	      //opcjonalne wyswietlanie surowej ramki RMC na dole -> dobrze przy debugu

	      /*
		  //wyswietlenie calosci bufforow
	      if (rmc_buffer[0] != '\0') {
	                //ramka RMC rozbita i wyswietlona od lini 4
	                SplitTextWithIndex(rmc_buffer, lines, 4);
	            }
	      if (gga_buffer[0] != '\0') {
	      	  	    //ramka GGA rozbita i wyswietlona od lini 5
	                SplitTextWithIndex(gga_buffer, lines, 5);
	            }
	       */

	      //wyświetlanie całości
	      for (int i = 0; i < MAX_LINES; i++) {
	          if (lines[i][0] != '\0') {
	              GFX_DrawString(0, i * 10, lines[i], WHITE, BLACK);
	          }
	      }

	      SSD1306_Display();
	      HAL_Delay(100);


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  // Włącz przerwania w NVIC dla USART2
  HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */


/* USER CODE BEGIN 4 */


//callback przerwania UART – odbior po 1 bajcie
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2) { //jesli przerwanie przychodzi z USART

        if(rx_byte != '\n' && rx_byte != '\r') {//dodajemy znak po znaku do bufora jesli jest zwykly
            if(gps_idx < GPS_BUFFER_SIZE - 1) {
                gps_buffer[gps_idx++] = rx_byte;
            }
        } else {
            if(gps_idx > 0) { //mamy jakiekolwiek dane w buforze - konczymy linie
                gps_buffer[gps_idx] = '\0';
                gps_line_ready = 1;
                gps_idx = 0;
            }
        }

        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);//ponowne uruchomienie odbioru
    }
}



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

