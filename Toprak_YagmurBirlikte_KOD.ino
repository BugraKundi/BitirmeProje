// Pin Tanımlamaları
const int toprakNemPin = A0;      // Analog Giriş
const int yagmurDigitalPin = D1;  // Dijital Giriş (GPIO5)

// Kalibrasyon Değerleri (Kendi sensörüne göre güncelleyebilirsin)
const int kuruDeger = 800; 
const int islakDeger = 300; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Dijital pini giriş olarak tanımlıyoruz
  pinMode(yagmurDigitalPin, INPUT);
  
  Serial.println("Çoklu Sensör Sistemi Başlatıldı...");
}

void loop() {
  // --- 1. TOPRAK NEMİ OKUMA (ANALOG) ---
  int hamDeger = analogRead(toprakNemPin);
  int nemYuzdesi = map(hamDeger, kuruDeger, islakDeger, 0, 100);
  nemYuzdesi = constrain(nemYuzdesi, 0, 100); // Değerleri %0-%100 arasında sınırla

  // --- 2. YAĞMUR DURUMU OKUMA (DİJİTAL) ---
  int yagmurVarMi = digitalRead(yagmurDigitalPin);

  // --- 3. SERİ PORTA YAZDIRMA ---
  Serial.print("Toprak Nemi: %");
  Serial.print(nemYuzdesi);
  Serial.print(" (Ham: ");
  Serial.print(hamDeger);
  Serial.print(") | ");

  Serial.print("Hava Durumu: ");
  if (yagmurVarMi == LOW) {
    Serial.println("[ YAĞMUR YAĞIYOR! ]");
    // İleride buraya yağmur yağdığında yapılmasını istediğin aksiyonları yazabilirsin
    // Örn: Lazerleri kapat, buzzerı sustur vb.
  } else {
    Serial.println("[ Hava Kuru ]");
  }

  delay(2000); // 2 saniyede bir güncelle
}