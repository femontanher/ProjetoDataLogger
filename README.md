# ProjetoDataLogger
Atividade N1 para a criação de um DataLogger.
Link wokwi: https://wokwi.com/projects/441016664910978049 

O projeto mede temperatura, umidade e luminosidade, exibe em LCD 16×2 I2C, mantém hora/data via RTC DS1307, sinaliza alertas por LEDs e buzzer, e registra na EEPROM leituras fora de faixa (buffer circular). Possui menu navegável por 4 botões para ajustes de preferências, unidades, limites e fuso (UTC). Contém todas as seguintes funcionalidades: 

- [x]  Armazenamento: Memória EEPROM para armazenar os dados coletados.
- [x]  Microcontrolador: Utilizar o ATMEGA 328P.
- [x]  Relógio de Tempo Real: Incluir um módulo RTC para marcar as entradas de dados com timestamps precisos.
- [x]  Interface de Usuário: Implementar um display LCD I2C para exibição de dados e status.
- [x]  Controles: Incluir teclas de input para navegação e configuração do dispositivo.
- [x]  Indicadores Visuais: Utilizar LEDs para indicar o status operacional e alertas.
- [x]  Alertas Sonoros: Incorporar um buzzer e alertas audíveis.
- [x]  Sensores: DHT 11 (temperatura e umidade) e LDR (luminosidade).
- [x]  Salvar as variaveis de configuração na EEPROM

Hardaware: 

| Componente               | Função                  | Pino(s) no código   |
| ------------------------ | ----------------------- | ------------------- |
| DHT11                    | Temperatura/Umidade     | `DHTPIN = 2`        |
| LDR                      | Luminosidade (0–100%)   | `ldrPin = A3`       |
| LCD 16×2 I2C (end. 0x27) | Exibição                | (SDA/SCL)           |
| RTC DS1307               | Relógio                 | (SDA/SCL)           |
| Buzzer                   | Sonoro                  | `buzzer = 8`        |
| LED Verde                | Atividade/“heartbeat”   | `LED_VERDE = 12`    |
| LED Laranja              | Atenção (fora da faixa) | `LED_LARANJA = 13`  |
| LED Vermelho             | Crítico (no máximo)     | `LED_VERMELHO = 11` |
| Botão UP                 | Navegação               | `BTN_UP = 4`        |
| Botão DOWN               | Navegação               | `BTN_DOWN = 5`      |
| Botão ENTER              | Seleção                 | `BTN_ENTER = 6`     |
| Botão BACK               | Voltar                  | `BTN_BACK = 7`      |

Indicadores de Led:
LED Verde - Todo registro realizado na eeprom dos valores lidos pelos sensores, o led verde irá piscar.

LED vermelho: pisca fixo ligado quando um valor atinge/excede o máximo permitido.

LED laranja: pisca quando há valores abaixo do mínimo ou acima do máximo, mas ainda não chegaram no ponto crítico (≥ máximo).

Nenhum LED aceso: todos os valores dentro da faixa configurada.



Além disso, conta com o seguinte menu personalizado:

INDICADORES
- [x]  Ir para a tela que registra as informações de temperatura, umidade e luminosidade
<img width="755" height="473" alt="image" src="https://github.com/user-attachments/assets/e693cd68-d416-4786-ae9d-62803a4e4346" />

PREFERÊNCIAS
- [x]  Ligar/Desligar a luz de fundo do display
<img width="727" height="480" alt="image" src="https://github.com/user-attachments/assets/e086841a-47e8-407c-99f6-ee282d9479e9" />

- [x]  Ativar/Desativar a opção de animacao do menu
<img width="723" height="467" alt="image" src="https://github.com/user-attachments/assets/44524351-06fd-44e8-952a-0ccff8584031" />

- [x]  Desativar som a cada clique do controle
<img width="726" height="468" alt="image" src="https://github.com/user-attachments/assets/be538981-8f38-42b7-932b-ae2eb9d5fd68" />

- [x]  Ativar/Desativar log Serial
<img width="744" height="468" alt="image" src="https://github.com/user-attachments/assets/7ca8886d-c800-4f21-822c-aa657a78a92a" />

UNIDADES
- [x]  Trocar o a unidade para F ou Celsius
<img width="741" height="466" alt="image" src="https://github.com/user-attachments/assets/6305e6bf-d4df-4ec6-85c2-8ac6e1cd60fb" />
<img width="724" height="474" alt="image" src="https://github.com/user-attachments/assets/d61d02c8-9cd5-49a0-b7a5-cd0c0d3cefb9" />

- [x]  Alterar o horário UTC
<img width="722" height="478" alt="image" src="https://github.com/user-attachments/assets/1d5372bb-204f-4d6e-84ad-f694655c455a" />

Observação: Está definido que o UTC pode ir de 14 até -12, o programa não deixa ultrapassar isso.

LIMITES
- [x]  Definir temperatura maxima
- [x]  Definir temperatura minima
- [x]  Definir umidade maxima
- [x]  Definir umidade minima
- [x]  Definir luminosidade maxima
- [x]  Definir luminosidade minima

Para todas as opções abaixo, não tem nenhum limite, o usuário consegue determinar o valor desejado.
<img width="748" height="460" alt="image" src="https://github.com/user-attachments/assets/19944f80-b0ae-43df-9562-852b93f53fa5" />

DESLIGAR

<img width="750" height="479" alt="image" src="https://github.com/user-attachments/assets/4f1fa479-14e5-4c01-81e7-e26d7af175ec" />
<img width="725" height="479" alt="image" src="https://github.com/user-attachments/assets/64a28ce0-8aa7-4d65-9211-f6de606f9e9b" />

