#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <DHT.h>

// ---------------- CONFIGURAÇÕES ----------------
#define DHTPIN 2
#define DHTTYPE DHT11
// ---------------- OBJETOS ----------------
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 RTC;

// ---------------- EEPROM ----------------
const int maxRecords = 80; 
const int recordSize = 10;  // 4 bytes timestamp + 2 temp + 2 umid + 2 lumi
int startAddress = 0;     // Início no endereço 0
int endAddress = startAddress + (maxRecords * recordSize) - 1;  // Calculando o último endereço da EEPROM
int currentAddress = startAddress;  // Início da gravação no startAddress
int lastLoggedMinute = -1;

bool newRecordSaved = true;

struct Config {
  float lumi_max;
  float lumi_min;
  float umidade_max;
  float umidade_min;
  float temp_max;
  float temp_min;
  char unidadePadrao; // 'C' ou 'F'
  int8_t UTC_OFFSET;   // pode ser negativo (-12 a +14)
  bool animacaoAtivado;
  bool som;
  bool log_serial;
};

const int addr_config = 810;

Config config;

// ---------------- PINS ----------------
#define ldrPin A3
#define buzzer 8
#define LED_VERMELHO 11 
#define LED_VERDE 12
#define LED_LARANJA 13

// ---- Botões ----
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_ENTER 6
#define BTN_BACK 7

// --------------- Menus     ------------

// Menu Principal
const char* menuPrincipal[] = {
  "INDICADORES",
  "PREFERENCIAS",
  "UNIDADES",
  "LIMITES",
  "DESLIGAR"
};
int tamMenuPrincipal = 5;

// Submenu Preferencias
const char* menuPreferencias[] = {
  "Luz de Fundo",
  "Animacao Intro",
  "Som Cliques",
  "Log Serial"
};
int tamMenuPreferencias = 4;

// Submenu Unidades
const char* menuUnidades[] = {
  "Trocar C/F",
  "Ajuste UTC"
};
int tamMenuUnidades = 2;

// Submenu Calibracao
const char* menuLimites[] = {
  "Luminosidade MAX",
  "Luminosidade MIN",
  "Umidade MAX",
  "Umidade MIN",
  "Temperatura MAX",
  "Temperatura MIN"
};

int tamMenuLimites = 6;

// -------- Estados de Menu --------
enum EstadoMenu { PRINCIPAL, PREFERENCIAS, UNIDADES, LIMITES, INDICADORES };
EstadoMenu estadoAtual = PRINCIPAL;
int selecao = 0;  // item selecionado

// Variáveis globais para controle do tempo do botão
unsigned long pressStart = 0;
bool enterPressing = false;

// Variáveis
int problema = 0;
unsigned long ultimoBlink = 0;
bool estadoLed = LOW;
unsigned long ultimoUpdateIndicadores = 0;
const unsigned long intervaloUpdate = 1000; // Atualiza a cada 1s
unsigned long lastDebounce = 0;
unsigned long debounceDelay = 200;
int enterOK = 0;  // Variável para verificar se o ENTER foi pressionado
int ajusteUTC = 0; // Variável para verificar se estamos no ajuste do UTC (0 = não, 1 = sim)
int ajusteLimite = 0;
int ajusteTEMP = 0;
int ajusteUMI = 0;
int desligarDispositivo = 0;
int limiteSelecionado = -1;  // Armazena qual variável está sendo ajustada

// Variável global para controlar a posição do scroll
int scrollPos = 0;

// ---- Pisca SOS (laranja) não-bloqueante ----
unsigned long sosBlinkLast = 0;
bool sosBlinkState = LOW;
const unsigned long sosBlinkInterval = 300; // ms

// ---- Estados de alerta ----
bool alertaForaDeFaixa = false; // qualquer métrica <min ou >max
bool alertaNoMaximo   = false;  // qualquer métrica >= max (prioridade vermelha)

void setup() {
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);
  dht.begin();
  RTC.begin();

  if (!RTC.isrunning()) {
    Serial.println("RTC parado, ajustando com a hora de compilacao (LOCAL)...");
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__))); // grava HORA LOCAL
  }
  
  EEPROM.begin();

  carregarConfig();

  if(config.animacaoAtivado == true){
    animacao(); 
  }

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_LARANJA, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  beepTecla();
  // Mostra menu inicial
  mostrarMenu(menuPrincipal, tamMenuPrincipal);
}

// ==================================================
void loop() {
  // --- Navegação de botões ---
  navegacaoBotoes();

  float temperatura = dht.readTemperature();
  float umidade     = dht.readHumidity();

  // --- lê luminosidade em % ---
  int valorLDR = analogRead(ldrPin);
  float luminosidade = map(valorLDR, 20, 250, 100, 0);
  luminosidade = constrain(luminosidade, 0, 100);

  // --- obtém tempo ajustado ---
  DateTime now = RTC.now();
  long epoch = now.unixtime() + (config.UTC_OFFSET * 3600);
  DateTime adjustedTime(epoch);

  verificarAlertasESinais(temperatura, umidade, luminosidade);

  // --- grava dados 1x por minuto ---
  if (adjustedTime.minute() != lastLoggedMinute) {
    lastLoggedMinute = adjustedTime.minute();

    // blink LED
    digitalWrite(LED_VERDE, HIGH); delay(200);
    digitalWrite(LED_VERDE, LOW);

    newRecordSaved = true;

    // lê sensores
    float umidade = dht.readHumidity();
    float temperatura = dht.readTemperature();

    if (!isnan(umidade) && !isnan(temperatura)) {
      if (temperatura < config.temp_min || temperatura > config.temp_max ||
        umidade < config.umidade_min || umidade > config.umidade_max ||
        luminosidade < config.lumi_min || luminosidade > config.lumi_max) {

        // salva como inteiros (2 casas decimais)
        int tempInt = (int)(temperatura * 100);
        int humiInt = (int)(umidade * 100);
        int lumiInt = (int)(luminosidade * 100);

        EEPROM.put(currentAddress, now.unixtime());
        EEPROM.put(currentAddress + 4, tempInt);
        EEPROM.put(currentAddress + 6, humiInt);
        EEPROM.put(currentAddress + 8, lumiInt);

        // Atualiza o endereço para o próximo registro
        getNextAddress();
      }
    }
  }
  
  // --- imprime log no Serial ---
  if (config.log_serial && newRecordSaved) {
    printLastRecord(epoch, temperatura, umidade, luminosidade);
    newRecordSaved = false;
  }
  delay(50);

  if (estadoAtual == INDICADORES) {
    atualizarIndicadores();
  }

  if (ajusteUTC == 1) {
    ajustarUTC();  // Mantém o ajuste ativo
    return;  // Evita que o resto do loop rode
  }

  if (ajusteLimite == 1) {
    ajustarLimite();
    return;  // Evita que o resto do loop rode
  }

  if (desligarDispositivo == 1){
    desligar();
    return;
  }
}

// -------- ALERTAS  --------
void verificarAlertasESinais(float tC, float h, float l) {
  // Se qualquer valor veio inválido, não mexe nos LEDs
  if (!isfinite(tC) || !isfinite(h) || !isfinite(l)) {
    digitalWrite(LED_LARANJA, LOW);
    digitalWrite(LED_VERMELHO, LOW);
    sosBlinkState = LOW;
    return;
  }

  bool noMax = (tC >= config.temp_max) || (h >= config.umidade_max) || (l >= config.lumi_max);
  bool foraFaixa = (tC < config.temp_min || tC > config.temp_max) ||
                   (h  < config.umidade_min || h  > config.umidade_max) ||
                   (l  < config.lumi_min    || l  > config.lumi_max);

  if (noMax) {
    digitalWrite(LED_VERMELHO, HIGH);     // crítico
    digitalWrite(LED_LARANJA, LOW);
    sosBlinkState = LOW;
  } else if (foraFaixa) {
    digitalWrite(LED_VERMELHO, LOW);      // atenção
    unsigned long agora = millis();
    if (agora - sosBlinkLast >= sosBlinkInterval) {
      sosBlinkLast = agora;
      sosBlinkState = !sosBlinkState;
      digitalWrite(LED_LARANJA, sosBlinkState);
    }
  } else {
    digitalWrite(LED_LARANJA, LOW);       // normal
    digitalWrite(LED_VERMELHO, LOW);
    sosBlinkState = LOW;
  }
}

void printLastRecord(long epoch, float temperatura, float umidade, float luminosidade) {
  somRegistroSerial();
  Serial.println("EEPROM (timestamp   |  temp   |  umid    |  lumi)");
  DateTime dt(epoch);
  Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
  Serial.print(" | ");
  Serial.print(temperatura); Serial.print(" C | ");
  Serial.print(umidade); Serial.print(" % | ");
  Serial.print(luminosidade); Serial.println(" %");
}

void get_log() {
  Serial.println("EEPROM (timestamp   |  temp   |  umid    |  lumi)");

  for (int address = startAddress; address < currentAddress; address += recordSize) {
    long timeStamp;
    int tempInt, humiInt, lumiInt;

    // Ler dados da EEPROM
    EEPROM.get(address, timeStamp);
    EEPROM.get(address + 4, tempInt);
    EEPROM.get(address + 6, humiInt);
    EEPROM.get(address + 8, lumiInt);

    float temperatura = tempInt / 100.0;
    float umidade = humiInt / 100.0;
    float luminosidade = lumiInt / 100.0;

    if (timeStamp != 0xFFFFFFFF) {
      DateTime dt = DateTime(timeStamp);
      Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
      Serial.print(" | ");
      Serial.print(temperatura); Serial.print(" C | ");
      Serial.print(umidade); Serial.print(" % | ");
      Serial.print(luminosidade); Serial.println(" %");
    }
  }
}

void getNextAddress() {
  currentAddress += recordSize;
  if (currentAddress >= (startAddress + maxRecords * recordSize)) {
    currentAddress = startAddress; // reinicia no início dos registros
  }
}

void dataHora() {
  DateTime now = RTC.now();
  long epoch = now.unixtime() + (config.UTC_OFFSET * 3600);
  DateTime adjustedTime(epoch);

  // Monta string com data e hora
  char buffer[20];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d",
          adjustedTime.day(),
          adjustedTime.month(),
          adjustedTime.year(),
          adjustedTime.hour(),
          adjustedTime.minute(),
          adjustedTime.second());

  String texto = String(buffer);

  // Tamanho máximo visível (8 colunas: do 8 até o 15)
  int larguraJanela = 8;

  // Se chegou ao fim, reinicia o scroll
  if (scrollPos > texto.length() - larguraJanela) {
    scrollPos = 0;
  }

  // Extrai pedaço do texto para exibir
  String visivel = texto.substring(scrollPos, scrollPos + larguraJanela);

  // Mostra no LCD, sempre a partir da coluna 8
  lcd.setCursor(8, 1);
  lcd.print(visivel);

  // Avança o scroll para a próxima chamada
  scrollPos++;
}

// ==================================================
// ---------------- FUNÇÕES da EEPROM ---------------
void carregarConfig() {
  EEPROM.get(addr_config, config);

  // Validação de dados (caso seja a primeira vez ou EEPROM esteja "suja")
  if (isnan(config.lumi_max) || isnan(config.lumi_min) ||
    isnan(config.umidade_max) || isnan(config.umidade_min) ||
    isnan(config.temp_max) || isnan(config.temp_min) ||
    (config.temp_min < -50 || config.temp_max > 100) ||
    (config.umidade_min < 0 || config.umidade_max > 100) ||
    (config.lumi_min < 0 || config.lumi_max > 100) ||
    (config.UTC_OFFSET < -12 || config.UTC_OFFSET > 14)) {

    Serial.println("EEPROM suja, carregando valores padrão...");

    config.lumi_max = 30.0;
    config.lumi_min = 0.0;
    config.umidade_max = 50.0;
    config.umidade_min = 30.0;
    config.temp_max = 50.0;
    config.temp_min = 15.0;
    config.unidadePadrao = 'C';
    config.UTC_OFFSET = -3;
    config.animacaoAtivado = true;
    config.som = true;
    config.log_serial = true;

    salvarConfig(); // grava os padrões na EEPROM
  }
}

void salvarConfig() {
  EEPROM.put(addr_config, config);
  Serial.println("Configurações salvas na EEPROM!");
}

// ==================================================
// ---------------- FUNÇÕES DO MENU ----------------
void mostrarMenu(const char* opcoes[], int tamanho) {
  lcd.clear();

  // --- Linha 1 (opção atual) ---
  lcd.setCursor(0, 0);

  // Se passar de 16 caracteres, faz scroll
  if (strlen(opcoes[selecao]) > 14) {  // 14 porque já tem "> "
    lcd.print(opcoes[selecao]);
  }else{
    lcd.print("> ");
    lcd.print(opcoes[selecao]);
  }

  // --- Linha 2 (próxima opção) ---
  int prox = (selecao + 1) % tamanho;
  lcd.setCursor(0, 1);
  lcd.print(" ");
  lcd.print(opcoes[prox]);
}

void navegarMenu(int direcao, const char* opcoes[], int tamanho) {
  selecao += direcao;
  if (selecao < 0) selecao = tamanho - 1;
  if (selecao >= tamanho) selecao = 0;
  mostrarMenu(opcoes, tamanho);
}

void selecionarOpcao() {
  switch (estadoAtual) {
    case PRINCIPAL:
      switch (selecao) {
        case 0: estadoAtual = INDICADORES; mostrarIndicadores(); break;
        case 1: estadoAtual = PREFERENCIAS; selecao = 0; mostrarMenu(menuPreferencias, tamMenuPreferencias); break;
        case 2: estadoAtual = UNIDADES; selecao = 0; mostrarMenu(menuUnidades, tamMenuUnidades); break;
        case 3: estadoAtual = LIMITES; selecao = 0; mostrarMenu(menuLimites, tamMenuLimites); break;
        case 4: iniciarDesligarSistema(); break;
      }
      break;

    case PREFERENCIAS:
      if (selecao == 0) Backlight();
      else if (selecao == 1) Animacao();
      else if (selecao == 2) Som();
      else if (selecao == 3) LogSerial();
      break;

    case UNIDADES:
      if (selecao == 0) trocarUnidadeTemp();
      else if (selecao == 1) {
        ajusteUTC = 1;
      }
      break;


    case LIMITES:
      if (selecao == 0) iniciarAjusteLimite(selecao);
      else if (selecao == 1) iniciarAjusteLimite(selecao);
      else if (selecao == 2) iniciarAjusteLimite(selecao);
      else if (selecao == 3) iniciarAjusteLimite(selecao);
      else if (selecao == 4) iniciarAjusteLimite(selecao);
      else if (selecao == 5) iniciarAjusteLimite(selecao);
      break;

    case INDICADORES:
      mostrarIndicadores();
      break;
  }
}

// ==================================================
// ---------------- AÇÕES DO MENU ------------------
void mostrarIndicadores() {
  lcd.clear();
  lcd.setCursor(0,0);

  atualizarIndicadores(); 
}

void Backlight() { 
  static bool ligado = true; ligado = !ligado; 
  if (ligado) lcd.backlight(); else lcd.noBacklight(); 
}

void Animacao() { 
  lcd.clear();
  if(config.animacaoAtivado == true){
    lcd.print("Intro Desativada!");
    for (int i = 0; i < 8 + strlen("Intro Desativada!"); i++) {
      delay(200);
      lcd.scrollDisplayLeft();
    }
    config.animacaoAtivado = false;
  }else{
    lcd.print("Intro Ativada!");
    for (int i = 0; i < 8 + strlen("Intro Ativada!"); i++) {
      delay(200);
      lcd.scrollDisplayLeft();
    }
    config.animacaoAtivado = true;
  }

  salvarConfig();
  delay(2000);
  mostrarMenu(menuPreferencias, tamMenuPreferencias);
}

void Som() { 
  if(config.som){
    lcd.clear();
    beepTecla();
    config.som = false;
    lcd.print("Som desativado!");
  }else{
    lcd.clear();
    beepTecla();
    config.som = true;
    lcd.print("Som Ativado!");
  }

  salvarConfig();
  delay(3000);
  mostrarMenu(menuPreferencias, tamMenuPreferencias);
}

void LogSerial() { 
  if(config.log_serial){
    config.log_serial = false;
    lcd.clear();
    lcd.print("Serial Desativada!");
  }else{
    config.log_serial = true;
    lcd.clear();
    lcd.print("Serial Ativada!");
  }
  
  salvarConfig();
  delay(3000);
  mostrarMenu(menuPreferencias, tamMenuPreferencias);
}

void trocarUnidadeTemp() { 
  if (config.unidadePadrao == 'C') {
    config.unidadePadrao = 'F'; 
    lcd.clear(); 
    lcd.print("Convertido p/ F");
  } else {
    config.unidadePadrao = 'C'; 
    lcd.clear(); 
    lcd.print("Convertido p/ C");
  }

  salvarConfig();
  delay(2000);
  mostrarMenu(menuUnidades, tamMenuUnidades);
}

void ajustarUTC() { 
  // Exibe a mensagem inicial
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ajustar UTC");

  // Exibe o valor atual do UTC
  lcd.setCursor(12, 0);
  lcd.print(config.UTC_OFFSET);

  // Checa se o enterOK é 0, significa que o botão de confirmação ainda não foi pressionado
  if (ajusteUTC == 1) {  // Só entra no loop de ajustes de UTC
    // Verifica o debounce antes de ler os botões
    if (millis() - lastDebounce > debounceDelay) {
      // Aumenta o UTC se o botão UP for pressionado
      if (digitalRead(BTN_UP) == LOW) {
        config.UTC_OFFSET++;
        if (config.UTC_OFFSET > 14) config.UTC_OFFSET = 14;   // Limite superior
        lastDebounce = millis();  // Reseta o debounce

        // Atualiza a tela com o novo valor de UTC
        lcd.setCursor(12, 0);
        lcd.print("  ");  // Apaga o valor anterior
        lcd.setCursor(12, 0);
        lcd.print(config.UTC_OFFSET);

        // Exibe a alteração no monitor serial
        Serial.print("UTC alterado: ");
        Serial.println(config.UTC_OFFSET);
      }

      // Diminui o UTC se o botão DOWN for pressionado
      if (digitalRead(BTN_DOWN) == LOW) {
        config.UTC_OFFSET--;
        if (config.UTC_OFFSET < -12) config.UTC_OFFSET = -12; // Limite inferior
        lastDebounce = millis();  // Reseta o debounce

        // Atualiza a tela com o novo valor de UTC
        lcd.setCursor(12, 0);
        lcd.print("  ");  // Apaga o valor anterior
        lcd.setCursor(12, 0);
        lcd.print(config.UTC_OFFSET);

        // Exibe a alteração no monitor serial
        Serial.print("UTC alterado: ");
        Serial.println(config.UTC_OFFSET);
      }
    }
  }

  // Verifica se o botão ENTER foi pressionado para avançar
  if (digitalRead(BTN_ENTER) == LOW) {
    ajusteUTC = 0; // Sair do ajuste UTC
    enterOK = 1;   // Permite navegar para outro menu
    salvarConfig();
    delay(2000);
    mostrarMenu(menuUnidades, tamMenuUnidades);  // Exibe o próximo menu
  }
}

void ajustarLimite() {
  lcd.clear();
  lcd.setCursor(0, 0);

  if(ajusteLimite == 1){
    switch (limiteSelecionado) {
      case 0:
        lcd.print("Lmax: ");
        lcd.print(config.lumi_max);
        break;
      case 1:
        lcd.print("Lmin: ");
        lcd.print(config.lumi_min);
        break;
      case 2:
        lcd.print("Umax: ");
        lcd.print(config.umidade_max);
        break;
      case 3:
        lcd.print("Umin: ");
        lcd.print(config.umidade_min);
        break;
      case 4:
        lcd.print("Tmax: ");
        lcd.print(config.temp_max);
        break;
      case 5:
        lcd.print("Tmin: ");
        lcd.print(config.temp_min);
        break;
    }

    // Ajuste com botões
    if (millis() - lastDebounce > debounceDelay) {
      if (digitalRead(BTN_UP) == LOW) {
        switch (limiteSelecionado) {
          case 0: config.lumi_max++; break;
          case 1: config.lumi_min++; break;
          case 2: config.umidade_max++; break;
          case 3: config.umidade_min++; break;
          case 4: config.temp_max++; break;
          case 5: config.temp_min++; break;
        }
        lastDebounce = millis();
      }

      if (digitalRead(BTN_DOWN) == LOW) {
        switch (limiteSelecionado) {
          case 0: config.lumi_max--; break;
          case 1: config.lumi_min--; break;
          case 2: config.umidade_max--; break;
          case 3: config.umidade_min--; break;
          case 4: config.temp_max--; break;
          case 5: config.temp_min--; break;
        }
        lastDebounce = millis();
      }
    }
  }

  // Finaliza ajuste com ENTER
  if (digitalRead(BTN_ENTER) == LOW) {
    salvarConfig();
    ajusteLimite = 0;
    enterOK = 1;
    limiteSelecionado = -1;
    delay(500);
    mostrarMenu(menuLimites, tamMenuLimites);
  }
}

void iniciarAjusteLimite(int selecao) {
  ajusteLimite = 1;
  enterOK = 0;
  limiteSelecionado = selecao;
}

void desligar() { 
  lcd.clear(); 
  lcd.print("Desligando...");
  Serial.print("Desligando...");

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(LED_LARANJA, LOW);

  delay(3000); 
  lcd.clear();
  lcd.noBacklight();
  delay(3000); 
}

void iniciarDesligarSistema(){
  desligarDispositivo = 1;
}

void atualizarIndicadores() {
  unsigned long agora = millis();
  if (agora - ultimoUpdateIndicadores < intervaloUpdate) return;
  ultimoUpdateIndicadores = agora;

  // --- Leituras ---
  float tempC = dht.readTemperature();
  float umidade = dht.readHumidity();
  int valorLDR = analogRead(ldrPin);
  float luminosidade = map(valorLDR, 20, 250, 100, 0);
  luminosidade = constrain(luminosidade, 0, 100);

  // --- Temperatura para exibição ---
  float temperatura = tempC;
  if (config.unidadePadrao == 'F') {
    temperatura = tempC * 9.0 / 5.0 + 32.0;
  }

  // --- LCD ---
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("T:"); lcd.print(temperatura, 0); lcd.write(byte(223)); lcd.print(config.unidadePadrao);
  lcd.setCursor(9,0);
  lcd.print(" U:"); lcd.print(umidade, 0); lcd.print("%");
  lcd.setCursor(0,1);
  lcd.print("L:"); lcd.print(luminosidade, 0); lcd.print("%");
  dataHora();
}

void SoS(){
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_LARANJA, HIGH);   // liga
    delay(200);               // espera 200 ms
    digitalWrite(LED_LARANJA, LOW);    // desliga
    delay(200);               // espera 200 ms
  }
}

void ProblemaIndicadores(int temProblema){
  if (temProblema == 1) {
    unsigned long agora = millis();
    if (agora - ultimoBlink >= 700) {  // troca a cada 700 ms
      ultimoBlink = agora;
      estadoLed = !estadoLed; // alterna HIGH/LOW
      digitalWrite(LED_VERMELHO, estadoLed);
    }
  } else {
    digitalWrite(LED_VERMELHO, LOW); // desliga se não tem problema
  }
}

void beepTecla() {
  if (config.som) {
    tone(buzzer, 1000, 30);
  }
}

void somRegistroSerial(){
  if (config.som) {
    tone(buzzer, 250, 100);
  }
}

// Lógica de navegação de menus - A navegação deve ser bloqueada enquanto ajusta o UTC
void navegacaoBotoes() {
  // Só permite navegação se não estiver no ajuste de UTC
  if ((ajusteUTC == 0) && (ajusteLimite == 0) && (desligarDispositivo == 0)) {
    if (digitalRead(BTN_UP) == LOW) {
      Serial.println("BOTÃO UP pressionado");  // debug
      beepTecla();
      if (estadoAtual == PRINCIPAL) navegarMenu(-1, menuPrincipal, tamMenuPrincipal);
      else if (estadoAtual == PREFERENCIAS) navegarMenu(-1, menuPreferencias, tamMenuPreferencias);
      else if (estadoAtual == UNIDADES) navegarMenu(-1, menuUnidades, tamMenuUnidades);
      else if (estadoAtual == LIMITES) navegarMenu(-1, menuLimites, tamMenuLimites);
      delay(200);
    }

    if (digitalRead(BTN_DOWN) == LOW) {
      Serial.println("BOTÃO DOWN pressionado");  // debug
      beepTecla();
      if (estadoAtual == PRINCIPAL) navegarMenu(1, menuPrincipal, tamMenuPrincipal);
      else if (estadoAtual == PREFERENCIAS) navegarMenu(1, menuPreferencias, tamMenuPreferencias);
      else if (estadoAtual == UNIDADES) navegarMenu(1, menuUnidades, tamMenuUnidades);
      else if (estadoAtual == LIMITES) navegarMenu(1, menuLimites, tamMenuLimites);
      delay(200);
    }

    if (digitalRead(BTN_ENTER) == LOW) {
      Serial.println("BOTÃO ENTER pressionado");  // debug
      beepTecla();
      selecionarOpcao();
      delay(200);
    }

    if (digitalRead(BTN_BACK) == LOW) {
      Serial.println("BOTÃO BACK pressionado -> VOLTAR MENU PRINCIPAL");
      beepTecla();
      estadoAtual = PRINCIPAL;
      selecao = 0;
      mostrarMenu(menuPrincipal, tamMenuPrincipal);
      delay(200); // debounce simples
    }
  } else {
    // Bloqueia a navegação até que o ajuste de UTC seja finalizado
    // Ou seja, enquanto ajusteUTC == 1, os botões de navegação ficam bloqueados
  }
}

void animacao(){
  // caracteres customizados
  byte pontaFoguete[8] = {
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000
  };
  
  byte comecoFoguete[8] = {
  B00000,
  B10100,
  B11110,
  B11111,
  B11111,
  B11110,
  B10100,
  B00000
  };
  
  byte meioFoguete[8] = {
  B00000,
  B00001,
  B11111,
  B11111,
  B11111,
  B11111,
  B00001,
  B00000
  };
  
  byte meioFoguete2[8] = {
  B10000,
  B11000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11000,
  B10000
  };
  
  byte meioFoguete3[8] = {
  B00000,
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B00000,
  B00000
  };
  
  // frames da chama
  byte finalFoguete1[8] = {
  B00001,
  B00111,
  B00011,
  B00001,
  B00011,
  B00111,
  B00011,
  B00001
  };
  
  byte finalFoguete2[8] = {
  B00001,
  B00111,
  B00011,
  B00001,
  B00001,
  B00011,
  B00111,
  B00001
  };
  
  byte finalFoguete3[8] = {
  B00001,
  B00111,
  B01111,
  B00111,
  B00111,
  B01111,
  B00111,
  B00001
  };
  
  tone(buzzer, 250, 100);
  lcd.setCursor(0,0); lcd.print("Enable IMU...");
  delay(2000);
  lcd.clear();
  tone(buzzer, 250, 100);
  lcd.setCursor(0,0); lcd.print("Press Launch");
  delay(2000);
  lcd.clear();

  lcd.createChar(0, pontaFoguete);
  lcd.createChar(1, comecoFoguete);
  lcd.createChar(2, meioFoguete);
  lcd.createChar(3, finalFoguete1); 
  lcd.createChar(4, meioFoguete2);
  lcd.createChar(5, meioFoguete3);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Decolando");

  for (int i = 0; i <= 19; i++) {
    int percent = map(i, 0, 19, 0, 100);
    lcd.setCursor(12,0);
    lcd.print(percent);
    lcd.print("%");

    // limpa linha de baixo
    lcd.setCursor(0,1);
    lcd.print("                ");

    // desenha foguete
    lcd.setCursor(i,1);   lcd.write(byte(2)); 
    lcd.setCursor(i+1,1); lcd.write(byte(1)); 
    lcd.setCursor(i+2,1); lcd.write(byte(0)); 
    lcd.setCursor(i-1,1); lcd.write(byte(5)); 
    lcd.setCursor(i-2,1); lcd.write(byte(4)); 

    // anima chama no final
    lcd.createChar(3, finalFoguete1);
    lcd.setCursor(i-3,1); lcd.write(byte(3));
    delay(120);

    lcd.createChar(3, finalFoguete2);
    lcd.setCursor(i-3,1); lcd.write(byte(3));
    delay(120);

    lcd.createChar(3, finalFoguete3);
    lcd.setCursor(i-3,1); lcd.write(byte(3));
    delay(120);

    // chama o som do foguete para este frame
    fogueteSoundStep(i);
  }

  delay(2000);
  lcd.clear();
   
  for(int i=0; i < 30; i++){
    // Céu estrelado
    int estrelaX = random(0, 16); // coluna aleatória
    int estrelaY = random(0, 2);  // linha aleatória
    char simbolos[] = {'.','*','o','+'};
    char s = simbolos[random(0,4)];
    
    lcd.setCursor(estrelaX, estrelaY);
    lcd.print(s);
  }
  
  delay(200);
}

void fogueteSoundStep(int i) {
  // Frequência base sobe com o foguete
  int baseFreq = map(i, 0, 19, 150, 1800);

  // Adiciona variação aleatória para simular turbulência
  int freq = baseFreq + random(-80, 80);

  // Toca o som com duração curta (acompanha o frame da animação)
  tone(buzzer, freq, 100);

  // Ruído extra: toque rápido em outra frequência para dar mais "força"
  if (i > 5) { // só depois que o foguete "ganha potência"
    tone(buzzer, freq + random(100, 400), 50);
  }
}
