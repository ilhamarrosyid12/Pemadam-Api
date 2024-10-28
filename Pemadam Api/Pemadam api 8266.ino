#include <DHT.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h> // Pastikan pustaka ini sudah diinstal

#define DHTPIN D7     // Pin untuk sensor DHT22
#define DHTTYPE DHT22 // Tipe sensor DHT

DHT dht(DHTPIN, DHTTYPE);

// Pin untuk sensor flame
const int flamePins[] = {D5, D4, D3, D2, D1};
// Pin untuk sensor MQ-7
#define pinSensor A0 // Pin untuk membaca sensor MQ-7
// Pin untuk buzzer dan relay
const int buzzerPin = D6;
#define RELAY_PIN D8  // Gunakan GPIO D8 sebagai pin kontrol relay

// EEPROM addresses
const int TEMP_ADDR = 0;      // Alamat EEPROM untuk suhu
const int HUMIDITY_ADDR = 4;  // Alamat EEPROM untuk kelembapan
const int CO_ADDR = 8;        // Alamat EEPROM untuk konsentrasi CO

long RL = 1000; // 1000 Ohm
long Ro = 830;  // 830 ohm (SILAHKAN DISESUAIKAN)

// WiFi credentials
const char* ssid = "SangatKencang";         // Ganti dengan SSID jaringan WiFi
const char* password = "sangatkencang";     // Ganti dengan password jaringan WiFi

// Firebase credentials
#define FIREBASE_HOST "https://skripsi-pemadam-api-default-rtdb.asia-southeast1.firebasedatabase.app" // URL Firebase Anda
#define FIREBASE_AUTH "AIzaSyA2W28xdpeFipeKilnLbj1q2ZM3l8gXmHM" // Ganti dengan kunci rahasia Firebase Anda

// Deklarasi objek FirebaseConfig dan FirebaseAuth
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// Fungsi untuk menyimpan data float ke EEPROM
void writeFloat(int address, float value) {
    byte *ptr = (byte *)(void *)&value;
    for (int i = 0; i < sizeof(value); i++) {
        EEPROM.write(address + i, *ptr++);
    }
}

// Fungsi untuk membaca data float dari EEPROM
float readFloat(int address) {
    float value = 0.0;
    byte *ptr = (byte *)(void *)&value;
    for (int i = 0; i < sizeof(value); i++) {
        *ptr++ = EEPROM.read(address + i);
    }
    return value;
}

void setup() {
    // Inisialisasi pin sensor flame
    for (int i = 0; i < 5; i++) {
        pinMode(flamePins[i], INPUT);
    }

    pinMode(buzzerPin, OUTPUT);        // Inisialisasi pin buzzer
    pinMode(RELAY_PIN, OUTPUT);        // Inisialisasi pin relay
    digitalWrite(RELAY_PIN, LOW);      // Relay dimatikan di awal

    Serial.begin(115200);              // Mulai komunikasi serial
    dht.begin();                       // Memulai sensor DHT

    // Koneksi ke jaringan WiFi
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
    firebaseConfig.api_key = FIREBASE_AUTH; // Kunci API untuk Firebase
    firebaseAuth.user.email = "YOUR_EMAIL"; // Jika menggunakan autentikasi email
    firebaseAuth.user.password = "YOUR_PASSWORD"; // Jika menggunakan autentikasi email

    // Inisialisasi Firebase
    Firebase.begin(&firebaseConfig, &firebaseAuth);

    // Membaca data dari EEPROM
    float lastTemperature = readFloat(TEMP_ADDR);
    float lastHumidity = readFloat(HUMIDITY_ADDR);
    float lastCO = readFloat(CO_ADDR);

    // Menampilkan data yang disimpan dari EEPROM
    Serial.print("Last Suhu: ");
    Serial.print(lastTemperature);
    Serial.print(" °C, Last Kelembapan: ");
    Serial.print(lastHumidity);
    Serial.print(" %, Last CO: ");
    Serial.print(lastCO);
    Serial.println(" ppm");
}

void loop() {
    // Membaca data dari sensor DHT22
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    // Mengecek apakah pembacaan DHT22 berhasil
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Gagal membaca dari sensor DHT!");
        return;
    }

    // Menyimpan suhu dan kelembapan terbaru ke EEPROM jika ada perubahan
    if (temperature != readFloat(TEMP_ADDR)) {
        writeFloat(TEMP_ADDR, temperature);
    }
    if (humidity != readFloat(HUMIDITY_ADDR)) {
        writeFloat(HUMIDITY_ADDR, humidity);
    }

    // Mengirim data ke Firebase
    Firebase.setFloat(firebaseData, "/temperature", temperature);
    Firebase.setFloat(firebaseData, "/humidity", humidity);

    // Menampilkan data suhu dan kelembapan ke Serial Monitor
    Serial.print("Suhu: ");
    Serial.print(temperature);
    Serial.print(" °C, Kelembapan: ");
    Serial.print(humidity);
    Serial.println(" %");

    // Mengecek status sensor flame
    bool flameDetected = false;
    for (int i = 0; i < 5; i++) {
        if (digitalRead(flamePins[i]) == HIGH) {
            flameDetected = true;
            Serial.print("Flame ");
            Serial.print(i + 1);  // Menampilkan nomor sensor flame (1-5)
            Serial.println(" aktif!"); // Keterangan jika sensor aktif
        }
    }

    // Membaca nilai dari sensor MQ-7
    int sensorvalue = analogRead(pinSensor); // Membaca nilai ADC dari sensor
    float VRL = sensorvalue * 5.00 / 1024;   // Mengubah nilai ADC menjadi nilai voltase (0 - 5.00 volt)

    float Rs = (5.00 * RL / VRL) - RL; // Menghitung Rs
    float ppm = 100 * pow(Rs / Ro, -1.53); // Menghitung konsentrasi CO dalam ppm

    // Menyimpan konsentrasi CO terbaru ke EEPROM jika ada perubahan
    if (ppm != readFloat(CO_ADDR)) {
        writeFloat(CO_ADDR, ppm);
    }

    // Mengirim data konsentrasi CO ke Firebase
    Firebase.setFloat(firebaseData, "/CO", ppm);

    // Menampilkan konsentrasi CO dalam ppm
    Serial.print("CO : ");
    Serial.print(ppm);
    Serial.println(" ppm");

    // Logika buzzer dan relay: Jika terdeteksi api atau gas berbahaya
    if (flameDetected || ppm > 400) {  // Jika api atau gas berbahaya terdeteksi
        Serial.println("Bahaya terdeteksi (Api atau Gas)! Mengaktifkan relay dan buzzer.");
        
        digitalWrite(buzzerPin, HIGH);  // Aktifkan buzzer
        digitalWrite(RELAY_PIN, HIGH);   // Aktifkan relay
        Serial.println("Relay aktif, menyiram...");

        delay(5000); // Relay menyala selama 5 detik

        digitalWrite(RELAY_PIN, LOW);   // Matikan relay setelah menyiram
        Serial.println("Relay mati.");
        
    } else {
        Serial.println("Semua aman. Mematikan relay dan buzzer.");
        digitalWrite(buzzerPin, LOW);   // Matikan buzzer
        digitalWrite(RELAY_PIN, LOW);    // Matikan relay jika tidak ada bahaya
    }

    delay(2000); // Tunggu 2 detik sebelum loop berikutnya
}
