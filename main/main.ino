/* Codigo para ESP32 para hacer la medición de todos los sensores
 * en conjunto. Los resultados de las mediciones son mostrados
 * por comunicación serial
 * 
 * NOTAS:
 * Para subir codigo a esp32, desconectar de fuente de alimentacion externa
 * 
 * TODOs:
 * - Calibracion:
 *   - Medir tension en terminales (pcb) del sensonr de uv 
 *   - Medir tension de salida del particulado
 * - Definir tiempo de sueño profundo de esp32
 * - Cambiar link de envio de datos 
*/

// Funciones sensores
#include "sensor_material_particulado.h"
#include "sensor_temp_hum.h"
#include "sensor_UV.h"

#include <Arduino.h>
#include <Adafruit_SGP30.h>
#include <WiFi.h>
#include <HTTPClient.h>


#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 10  // Tiempo en segundos

// Credenciales WiFi
const char* ssid = "";
const char* password = "";

const char* apiUrl = "https://api.thingspeak.com/update?api_key=L62DGBD7ZYY9K02O";

Adafruit_SGP30 sgp;
int counter = 0;
int counter1 = 0;

void setup(){
  Serial.begin(115200);

  // -------------- Setup Sensores --------------
  setupSensorMaterialParticulado();
  setupSensorTempHum();
  setupSensorUV();
  // CO2
  if (!sgp.begin()) { // Alerta por si la ESP no detecta el sensor
    Serial.println("Sensor de CO2 no encontrado.");
  }

  // Conexion WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a la red: ");
  Serial.println(ssid);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Conexion WiFi establecida!");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());
  
  delay(2000);

}

float material_particulado, ultra_violeta, humedad, temperatura, co2;
float total_MP, total_UV, total_H, total_T, total_CO2;
int nMuestras = 3;

void medir(){
  material_particulado = leerSensorMaterialParticulado(); // tiempo de ejecucion de 1 seg (N*10 ms)
  ultra_violeta = leerSensorUV();
  humedad = leerSensorHumedad();
  temperatura = leerSensorTemperatura();
  co2 = leerCO2();
  delay(2000);
}

void loop(){
  total_MP = 0; total_UV = 0; total_H = 0; total_T = 0; total_CO2 = 0; // reset counters
  
  for(int i = 0; i < nMuestras; i++){  // Tomar nMuestras y luego las envia
    medir();
    // Volver a Medir cuando hay error
    while(isnan(temperatura)or isnan(humedad) or isnan(ultra_violeta)or co2 == -1 or material_particulado == 0){
      medir();
    }
    // Sumar para promediar
    total_MP += material_particulado;
    total_UV += ultra_violeta;
    total_H += humedad;
    total_T += temperatura;
    total_CO2 += co2;
  }
  // Promediar y luego enviar a thingspeak
  material_particulado = total_MP / nMuestras;
  ultra_violeta = total_UV / nMuestras;
  humedad = total_H / nMuestras;
  temperatura = total_T / nMuestras;
  co2 = total_CO2 / nMuestras;
  
  Serial.println("");
  Serial.println("-----------------------------------------------");
  Serial.println("------------------- RESUMEN -------------------");
  Serial.println("Material Particulado: " + String(material_particulado) + " ug/m3" +
                 " Ultra Violeta: " + String(ultra_violeta) + " mW/m2" +
                 " Humedad: " + String(humedad) + " %" +
                 " Temperatura: " + String(temperatura) + " ºC" +
                 " C02: " + String(co2) + " ppm");
  Serial.println("");
  delay(1000);

  if(WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    //Linea de envio de datos
    String data = "&field1=" + String(temperatura) +
                  "&field2=" + String(humedad) +
                  "&field3=" + String(material_particulado) +
                  "&field4=" + String(ultra_violeta) +
                  "&field5=" + String(co2);
    String query = apiUrl + data;
    http.begin(query.c_str());
    int httpResponseCode = http.GET();
             
    Serial.print("Codigo de respuesta: ");
    Serial.println(httpResponseCode);
    http.end();
  }
  else{
    Serial.println("Conexion WiFi no disponible");
  }
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);  // Inicia conteo para despertar
  esp_deep_sleep_start();  

}

float leerCO2(){
  if (!sgp.IAQmeasure()){  // Alerta en caso de error
    Serial.println("Medición CO2 fallida.");
    return -1;
  }
  if (!sgp.IAQmeasureRaw()){ // Alerta en caso de error
    Serial.println("Medición CO2 sin procesar fallida.");
    return -1;
  }
  
  delay(1000);
  counter++;

  // Revisar funcionalidad en la documentación
  if (counter == 30){ // Contador para (LLENAR)
    counter = 0;
    uint16_t TVOC_base, eCO2_base;
    if (!sgp.getIAQBaseline(&eCO2_base, &TVOC_base)){ // Indicar errores en caso de falla en la medicion
      Serial.println("Error al obtener las lecturas de referencia.");
      return -1;
    }
  }
  return sgp.eCO2;
}
