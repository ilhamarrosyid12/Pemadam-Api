#include <DHT.h>
#include <EEPROM.h>
#include <WiFi.h>           // Gunakan WiFi.h
#include <FirebaseESP32.h>   // Pastikan library ini terinstall

#define DHTPIN 15           // GPIO 15 untuk sensor DHT22
#define DHTTYPE DHT22       // Tipe sensor DHT

DHT dht(DHTPIN, DHTTYPE);

// Pin untuk sensor api
const int flamePins[] = {32, 33, 25, 26, 27};  // GPIO untuk sensor api
// Pin untuk sensor MQ-7
#define pinSensor 34        // GPIO 34 untuk sensor MQ-7
// Pin untuk buzzer dan relay
const int buzzerPin = 14;   // GPIO 14 untuk buzzer
#define RELAY_PIN 13        // GPIO 13 untuk relay

// Alamat EEPROM
const int TEMP_ADDR = 0;       // Alamat EEPROM untuk suhu
const int HUMIDITY_ADDR = 4;   // Alamat EEPROM untuk kelembapan
const int CO_ADDR = 8;         // Alamat EEPROM untuk konsentrasi CO

long RL = 1000;                // 1000 Ohm
long Ro = 830;                 // 830 Ohm (Sesuaikan sesuai sensor)

// Kredensial WiFi
const char* ssid = "SangatKencang";
const char* password = "sangatkencang";

// Kredensial Firebase
#define FIREBASE_HOST "https://skripsi-pemadam-api-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "AIzaSyA2W28xdpeFipeKilnLbj1q2ZM3l8gXmHM"

// Deklarasi Firebase
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// Fungsi untuk menyimpan dan membaca float dari EEPROM
void writeFloat(int address, float value) {
    byte *ptr = (byte *)(void *)&value;
    for (int i = 0; i < sizeof(value); i++) {
        EEPROM.write(address + i, *ptr++);
    }
}

float readFloat(int address) {
    float value = 0.0;
    byte *ptr = (byte *)(void *)&value;
    for (int i = 0; i < sizeof(value); i++) {
        *ptr++ = EEPROM.read(address + i);
    }
    return value;
}

void setup() {
    // Inisialisasi pin sensor api
    for (int i = 0; i < 5; i++) {
        pinMode(flamePins[i], INPUT);
    }

    pinMode(buzzerPin, OUTPUT);        // Inisialisasi pin buzzer
    pinMode(RELAY_PIN, OUTPUT);        // Inisialisasi pin relay
    digitalWrite(RELAY_PIN, LOW);      // Relay dimatikan pada awal

    Serial.begin(115200);              // Memulai komunikasi Serial
    dht.begin();                       // Memulai sensor DHT

    // Koneksi ke WiFi
    WiFi.begin(ssid, password);
    Serial.print("Menghubungkan ke WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nTerhubung ke jaringan WiFi");
    Serial.print("Alamat IP: ");
    Serial.println(WiFi.localIP());

    // Konfigurasi Firebase
    firebaseConfig.host = FIREBASE_HOST;
    firebaseConfig.api_key = FIREBASE_AUTH;

    // Inisialisasi Firebase
    Firebase.begin(&firebaseConfig, &firebaseAuth);

    // Membaca data dari EEPROM
    float lastTemperature = readFloat(TEMP_ADDR);
    float lastHumidity = readFloat(HUMIDITY_ADDR);
    float lastCO = readFloat(CO_ADDR);

    Serial.print("Suhu terakhir: ");
    Serial.print(lastTemperature);
    Serial.print(" °C, Kelembapan terakhir: ");
    Serial.print(lastHumidity);
    Serial.print(" %, CO terakhir: ");
    Serial.print(lastCO);
    Serial.println(" ppm");
}

void loop() {
    // Membaca data dari sensor DHT22
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Gagal membaca dari sensor DHT!");
        return;
    }

    if (temperature != readFloat(TEMP_ADDR)) {
        writeFloat(TEMP_ADDR, temperature);
    }
    if (humidity != readFloat(HUMIDITY_ADDR)) {
        writeFloat(HUMIDITY_ADDR, humidity);
    }

    Firebase.setFloat(firebaseData, "/temperature", temperature);
    Firebase.setFloat(firebaseData, "/humidity", humidity);

    Serial.print("Suhu: ");
    Serial.print(temperature);
    Serial.print(" °C, Kelembapan: ");
    Serial.print(humidity);
    Serial.println(" %");

    bool flameDetected = false;
    for (int i = 0; i < 5; i++) {
        if (digitalRead(flamePins[i]) == HIGH) {
            flameDetected = true;
            Serial.print("Api terdeteksi di sensor ");
            Serial.println(i + 1);
        }
    }

    int sensorvalue = analogRead(pinSensor);
    float VRL = sensorvalue * 3.3 / 4095;

    float Rs = (3.3 * RL / VRL) - RL;
    float ppm = 100 * pow(Rs / Ro, -1.53);

    if (ppm != readFloat(CO_ADDR)) {
        writeFloat(CO_ADDR, ppm);
    }

    Firebase.setFloat(firebaseData, "/CO", ppm);

    Serial.print("Konsentrasi CO: ");
    Serial.print(ppm);
    Serial.println(" ppm");

    if (flameDetected || ppm > 400) {
        Serial.println("Bahaya terdeteksi (Api atau Gas)! Mengaktifkan relay dan buzzer.");
        
        digitalWrite(buzzerPin, HIGH);  
        digitalWrite(RELAY_PIN, HIGH);  

        delay(5000);

        digitalWrite(RELAY_PIN, LOW);   
        Serial.println("Relay dimatikan.");
        
    } else {
        Serial.println("Keadaan aman. Mematikan relay dan buzzer.");
        digitalWrite(buzzerPin, LOW);   
        digitalWrite(RELAY_PIN, LOW);   
    }

    delay(2000);
}
