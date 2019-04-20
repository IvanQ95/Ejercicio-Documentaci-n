#include "call.h"

/**********************************************************
Métodos que comprueban el estado de la llamada.
Regreso:
      CALL_NONE               - No hay actividad de llamada.
      CALL_INCOM_VOICE        - Voz entrante.
      CALL_ACTIVE_VOICE       - Voz activa.
      CALL_NO_RESPONSE        - No hay respuesta al comando AT.
      CALL_COMM_LINE_BUSY     - La línea de comunicación no es gratuita.
**********************************************************/
byte CallGSM::CallStatus(void)
{
     byte ret_val = CALL_NONE;

     if (CLS_FREE != gsm.GetCommLineStatus()) return (CALL_COMM_LINE_BUSY);
     gsm.SetCommLineStatus(CLS_ATCMD);
     gsm.SimpleWriteln(F("AT+CPAS"));

          // 5 segundos de espera para el inicio de la comunicación..
          // 50 msegundos de espera entre caracteres.
      
     if (RX_TMOUT_ERR == gsm.WaitResp(5000, 50)) {
          // No se recibió nada (RX_TMOUT_ERR)
          // -----------------------------------
          ret_val = CALL_NO_RESPONSE;
     } else {
          // Se recibió algo pero ¿Qué se recibió?
          // ---------------------------------------------
          // Listo (El dispositivo permite comandos desde TA/TE).
          // <CR><LF>+CPAS: 0<CR><LF> <CR><LF>OK<CR><LF>
          // No disponible (El dispositivo no permite comandos de TA/TE).
          // <CR><LF>+CPAS: 1<CR><LF> <CR><LF>OK<CR><LF>
          // Descononocido (No se garantiza que el dispositivo responda a las instrucciones).
          // <CR><LF>+CPAS: 2<CR><LF> <CR><LF>OK<CR><LF> - NO CALL
          // Timbre.
          // <CR><LF>+CPAS: 3<CR><LF> <CR><LF>OK<CR><LF> - NO CALL
          // Llamada en proceso.
          // <CR><LF>+CPAS: 4<CR><LF> <CR><LF>OK<CR><LF> - NO CALL
          if(gsm.IsStringReceived("+CPAS: 0")) {
               // Listo - no hay llamada.
               // ------------------------
               ret_val = CALL_NONE;
          } else if(gsm.IsStringReceived("+CPAS: 3")) {
               // Llamada entrante.
               // --------------
               ret_val = CALL_INCOM_VOICE;
          } else if(gsm.IsStringReceived("+CPAS: 4")) {
               // Llamada activa.
               // -----------
               ret_val = CALL_ACTIVE_VOICE;
          }
     }

     gsm.SetCommLineStatus(CLS_FREE);
     return (ret_val);

}

/**********************************************************
El método comprueba el estado de la llamada (Entrante o activa) y realiza la 
autorización con el rango de posiciones SIM especificadas.
phone_number: es un puntero donde se colocará la cadena de número de telefono 
              de la llamada actual por lo tanto, el espacio para la cadena del
              número de teléfono debe estar reservado (Ver ejemplo).
first_authorized_pos: Es la posición inicial de la agenda de la SIM donde comienza 
                      el proceso de autorización.
last_authorized_pos:  Es la última posición de la agenda de la SIM donde finaliza 
                      el proceso de autorización.
                      
                      Nota(importante):
                      ================
             En el caso first_authorized_pos=0 y también last_authorized_pos=0,
             el número de teléfono entrante recibido NO está autorizado en absoluto,
             por lo que cada entrada se considera autorizada (CALL_INCOM_VOICE_NOT_AUTH se devuelve)
Regreso:
      CALL_NONE                   - No hay actividad de llamada.
      CALL_INCOM_VOICE_AUTH       - Voz entrante - Autorizada.
      CALL_INCOM_VOICE_NOT_AUTH   - Voz entrante - No autorizada.
      CALL_ACTIVE_VOICE           - Voz activa.
      CALL_INCOM_DATA_AUTH        - Llamada de datos entrante - Autorizada.
      CALL_INCOM_DATA_NOT_AUTH    - Llamada de datos entrante - No autorizada.
      CALL_ACTIVE_DATA            - Llamada de datos activa.
      CALL_NO_RESPONSE            - No hay respuesta al comando AT.
      CALL_COMM_LINE_BUSY         - La línea de comunicación no es gratuita.
**********************************************************/
byte CallGSM::CallStatusWithAuth(char *phone_number,
                                 byte first_authorized_pos, byte last_authorized_pos)
{
     byte ret_val = CALL_NONE;
     byte search_phone_num = 0;
     byte i;
     byte status;
     char *p_char;
     char *p_char1;

     phone_number[0] = 0x00;  // No hay número de teléfono hasta ahora.
     if (CLS_FREE != gsm.GetCommLineStatus()) return (CALL_COMM_LINE_BUSY);
     gsm.SetCommLineStatus(CLS_ATCMD);
     gsm.SimpleWriteln(F("AT+CLCC"));

                  // 5 segundos de espera para el inicio de la comunicación.
                  // Además hay un tiempo máximo de 1500 msegundos de espera entre caracteres.
     gsm.RxInit(5000, 1500);
                  // El tiempo de espera ha finalizado.
     do {
          if (gsm.IsStringReceived("OK\r\n")) {
                 // Perfecto - Tenemos una respuesta pero debemos definir de que tipo:

                // No hay ninguna llamada:
                // <CR><LF>OK<CR><LF>
                // Hay al menos una llamada:
                // +CLCC: 1,1,4,0,0,"+420XXXXXXXXX",145<CR><LF>
                // <CR><LF>OK<CR><LF>
               status = RX_FINISHED;
               break; 
                // En ésta instancia termina de recibir e inmediatamente vamos 
                //a revisar la respuesta.
          }
          status = gsm.IsRxFinished();
     } while (status == RX_NOT_FINISHED);

                // Ahora es necesario generar un tiempo de espera de 30 msegundos antes 
                //del siguiente comando AT.
     delay(30);

     if (status == RX_FINISHED) {
                // Se recibió algo pero ¿Qué se recibió?
                // Ejemplo: //+CLCC: 1,1,4,0,0,"+420XXXXXXXXX",145
          // ---------------------------------------------
          if(gsm.IsStringReceived("+CLCC: 1,1,4,0,0")) {
               // Llamada de voz entrante - no autorizada hasta ahora.
               // -------------------------------------------
               search_phone_num = 1;
               ret_val = CALL_INCOM_VOICE_NOT_AUTH;
          } else if(gsm.IsStringReceived("+CLCC: 1,1,4,1,0")) {
               // Llamada de datos entrante - no autorizada hasta ahora.
               // ------------------------------------------
               search_phone_num = 1;
               ret_val = CALL_INCOM_DATA_NOT_AUTH;
          } else if(gsm.IsStringReceived("+CLCC: 1,0,0,0,0")) {
               // Llamada de voz activa - GSM es el que llama.
               // ----------------------------------
               search_phone_num = 1;
               ret_val = CALL_ACTIVE_VOICE;
          } else if(gsm.IsStringReceived("+CLCC: 1,1,0,0,0")) {
               // Llamada de voz activa - GSM es el oyente.
               // -----------------------------------
               search_phone_num = 1;
               ret_val = CALL_ACTIVE_VOICE;
          } else if(gsm.IsStringReceived("+CLCC: 1,1,0,1,0")) {
               // Llamada DATOS activa - GSM es oyente.
               // ----------------------------------
               search_phone_num = 1;
               ret_val = CALL_ACTIVE_DATA;
          } else if(gsm.IsStringReceived("+CLCC:")) {
               // Otra cadena no es importante para nosotros - por ejemplo, Módulo GSM activar llamada, etc.
               // IMPORTANTE - Cada respuesta +CLCC:xx tambien tiene al final la cadena <CR><LF>OK<CR><LF>.
               ret_val = CALL_OTHERS;
          } else if(gsm.IsStringReceived("OK")) {
               // Solo "OK" => No hay actividad de llamada.
               // --------------------------------------
               ret_val = CALL_NONE;
          }


          // Ahora buscaremos el número de teléfono.
          if (search_phone_num) {
               // Extraer cadena de número de teléfono.
               // ---------------------------
               p_char = strchr((char *)(gsm.comm_buf),'"');
               p_char1 = p_char+1; 
                // Estamos en el primer número de teléfono.
               p_char = strchr((char *)(p_char1),'"');
               if (p_char != NULL) {
                    *p_char = 0; 
                //Fin de la cadena.
                    strcpy(phone_number, (char *)(p_char1));
                    Serial.print("ATTESO: ");
                    Serial.println(phone_number);
               } else
                    //Serial.println(gsm.comm_buf);
                    Serial.println("NULL");

               if ( (ret_val == CALL_INCOM_VOICE_NOT_AUTH)
                         || (ret_val == CALL_INCOM_DATA_NOT_AUTH)) {

                    if ((first_authorized_pos == 0) && (last_authorized_pos == 0)) {
                         // No se requiere autorización o que la autorización es correcta.
                         // -------------------------------------------------------------
                         if (ret_val == CALL_INCOM_VOICE_NOT_AUTH) ret_val = CALL_INCOM_VOICE_AUTH;
                         else ret_val = CALL_INCOM_DATA_AUTH;
                    } else {
                         // Hacer la autorización.
                         // ------------------
                         gsm.SetCommLineStatus(CLS_FREE);
                         for (i = first_authorized_pos; i <= last_authorized_pos; i++) {
                              if (gsm.ComparePhoneNumber(i, phone_number)) {
                                   // Los números de teléfono son idénticos.
                                   // La autorización es correcta.
                                   // ---------------------------
                                   if (ret_val == CALL_INCOM_VOICE_NOT_AUTH) ret_val = CALL_INCOM_VOICE_AUTH;
                                   else ret_val = CALL_INCOM_DATA_AUTH;
                                   break;  
                                   // Aquí termina la autorización.
                              }
                         }
                    }
               }
          }

     } else {
          // Nada fue recibido(RX_TMOUT_ERR)
          // -----------------------------------
          ret_val = CALL_NO_RESPONSE;
     }

     gsm.SetCommLineStatus(CLS_FREE);
     return (ret_val);
}

/**********************************************************
Método recoge un retorno de llamada entrante:
**********************************************************/
void CallGSM::PickUp(void)
{
     //if (CLS_FREE != gsm.GetCommLineStatus()) return;
     //gsm.SetCommLineStatus(CLS_ATCMD);
     gsm.SendATCmdWaitResp("ATA", 10, 10, "OK", 3);
     gsm.SimpleWriteln("ATA");
     //gsm.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method hangs up incoming or active call
return:
**********************************************************/
void CallGSM::HangUp(void)
{
     //if (CLS_FREE != gsm.GetCommLineStatus()) return;
     //gsm.SetCommLineStatus(CLS_ATCMD);
     gsm.SendATCmdWaitResp("ATH", 500, 100, "OK", 5);
     //gsm.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method calls the specific number
number_string: pointer to the phone number string
               e.g. gsm.Call("+420123456789");
**********************************************************/
void CallGSM::Call(char *number_string)
{
     if (CLS_FREE != gsm.GetCommLineStatus()) return;
     gsm.SetCommLineStatus(CLS_ATCMD);
     // ATDxxxxxx;<CR>
     gsm.SimpleWrite(F("ATD"));
     gsm.SimpleWrite(number_string);
     gsm.SimpleWriteln(F(";"));
     // 10 sec. for initial comm tmout
     // 50 msec. for inter character timeout
     gsm.WaitResp(10000, 50);
     gsm.SetCommLineStatus(CLS_FREE);
}

/**********************************************************
Method calls the number stored at the specified SIM position
sim_position: position in the SIM <1...>
              e.g. gsm.Call(1);
**********************************************************/
void CallGSM::Call(int sim_position)
{
     if (CLS_FREE != gsm.GetCommLineStatus()) return;
     gsm.SetCommLineStatus(CLS_ATCMD);
     // ATD>"SM" 1;<CR>
     gsm.SimpleWrite(F("ATD>\"SM\" "));
     gsm.SimpleWrite(sim_position);
     gsm.SimpleWriteln(F(";"));

     // 10 sec. for initial comm tmout
     // 50 msec. for inter character timeout
     gsm.WaitResp(10000, 50);

     gsm.SetCommLineStatus(CLS_FREE);
}

void CallGSM::SendDTMF(char *number_string, int time)
{
     if (CLS_FREE != gsm.GetCommLineStatus()) return;
     gsm.SetCommLineStatus(CLS_ATCMD);

     gsm.SimpleWrite(F("AT+VTD="));
     gsm.SimpleWriteln(time);
     gsm.WaitResp(1000, 100, "OK");

     gsm.SimpleWrite(F("AT+VTS=\""));
     gsm.SimpleWrite(number_string);
     gsm.SimpleWriteln(F("\""));

     gsm.WaitResp(5000, 100, "OK");
     gsm.SetCommLineStatus(CLS_FREE);
}

void CallGSM::SetDTMF(int DTMF_status)
{
     if(DTMF_status==1)
          gsm.SendATCmdWaitResp("AT+DDET=1", 500, 50, "OK", 5);
     else
          gsm.SendATCmdWaitResp("AT+DDET=0", 500, 50, "OK", 5);
}


char CallGSM::DetDTMF()
{
     char *p_char;
     char *p_char1;
     char dtmf_char='-';
     gsm.WaitResp(1000, 500);
     {
          //Serial.print("BUF: ");
          //Serial.println((char *)gsm.comm_buf);
          //Serial.println("end");
          p_char = strstr((char *)(gsm.comm_buf),"+DTMF:");
          if (p_char != NULL) {
               p_char1 = p_char+6;  //we are on the first char of BCS
               dtmf_char = *p_char1;
          }
     }
     return dtmf_char;
}
