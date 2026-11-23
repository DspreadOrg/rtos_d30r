//-----------------------------------------------------------------------------
// @file    : i18n_pt.c
// @brief   : Portuguese (pt) language resource
// @details : The English source text is the lookup key, see LibLanguage.h.
//            A missing entry falls back to the English key, so only the
//            translated strings have to be listed here.
// @author  :
// @version :
// @date    :
// @history :
//-----------------------------------------------------------------------------
#include "LibLanguage.h"

//-----------------------------------------------------------------------------
// Local variable
//-----------------------------------------------------------------------------
static const LIB_LANG_ENTRY_T s_ptEntries[] =
{
    // Page title
    { "MAIN MENU",                  "MENU PRINCIPAL" },
    { "SETTINGS",                   "CONFIGURAÇÕES" },
    { "TOOLS",                      "FERRAMENTAS" },
    { "POWER",                      "ENERGIA" },
    { "LANGUAGE",                   "IDIOMA" },
    { "ABOUT",                      "SOBRE" },
    { "DEVICE INFO",                "INFO. DO DISPOSITIVO" },
    { "DATE & TIME",                "DATA E HORA" },
    { "NETWORK",                    "REDE" },
    { "PRINT TEST",                 "TESTE DE IMPRESSÃO" },
    { "SCAN",                       "DIGITALIZAR" },
    { "TEST",                       "TESTE" },
    { "E-SIGNATURE",                "ASSINATURA ELETRÔNICA" },
    { "TMS UPDATE",                 "ATUALIZAÇÃO TMS" },
    { "PAYMENT",                    "PAGAMENTO" },
    { "MAINTENANCE",                "MANUTENÇÃO" },
    { "WIFI",                       "WIFI" },
    { "WiFi NETWORKS",              "REDES WIFI" },
    { "WiFi DETAILS",               "DETALHES WIFI" },
    { "EMV PAYMENT",                "PAGAMENTO EMV" },

    // Home page
    { "Payment",                    "PAGAMENTO" },
    { "Card Payment",               "Pagamento com cartão" },
    { "Print",                      "IMPRIMIR" },
    { "Receipt / Test",             "Recibo / Teste" },
    { "Network",                    "REDE" },
    { "4G / WiFi Setup",            "Config. 4G / WiFi" },
    { "TMS",                        "TMS" },
    { "Remote Update",              "Atualização remota" },
    { "Settings",                   "CONFIGURAÇÕES" },
    { "Display / Time / Lang",      "Tela / Hora / Idioma" },
    { "Device",                     "DISPOSITIVO" },
    { "PN / SN / Ver",              "PN / SN / Ver" },
    { "Test",                       "TESTE" },
    { "MSR / IC / NFC / QR",        "MSR / IC / NFC / QR" },
    { "About",                      "SOBRE" },
    { "Version Info",               "Informação de versão" },
    { "More",                       "MAIS" },
    { "Tools",                      "Ferramentas" },
    { "Press 1-9 or tap a card to enter", "Pressione 1-9 ou toque um cartão" },

    // Menu pages
    { "AP List",                    "Lista de AP" },
    { "Scan and connect",           "Buscar e conectar" },
    { "Connection Details",         "Detalhes da conexão" },
    { "IP / mask / gateway",        "IP / máscara / gateway" },
    { "WiFi",                       "WiFi" },
    { "Wireless setup",             "Config. sem fio" },
    { "Network Settings",           "Config. de rede" },
    { "4G / WiFi mode",             "Modo 4G / WiFi" },
    { "Tap an item, or use UP / DOWN + ENTER", "Toque um item ou use CIMA / BAIXO + ENTER" },

    // Tools page
    { "Maintenance Mode",           "Modo de manutenção" },
    { "Memory Monitor",             "Monitor de memória" },
    { "Firmware download",          "Download de firmware" },
    { "Toggle RAM overlay",         "Alternar sobreposição de RAM" },

    // Power page
    { "Power Off",                  "Desligar" },
    { "Restart",                    "Reiniciar" },
    { "Cancel",                     "Cancelar" },
    { "Use UP / DOWN and ENTER, or tap an item", "Use CIMA / BAIXO e ENTER, ou toque um item" },

    // Language page
    { "Selected",                   "Selecionado" },
    { "Saved. Restart the device to apply.", "Salvo. Reinicie o dispositivo para aplicar." },
    { "Tap a language to select",   "Toque um idioma para selecionar" },
    { "Failed to switch language",  "Falha ao trocar o idioma" },
    { "Input",                      "Entrada" },

    // Settings page
    { "Brightness",                 "Brilho" },
    { "Buzzer",                     "Biper" },
    { "Sleep Time",                 "Tempo de sono" },
    { "Date & Time",                "Data e hora" },
    { "Language",                   "Idioma" },
    { "ON",                         "ATIVO" },
    { "OFF",                        "DESATIVADO" },

    // About page
    { "OS",                         "SO" },
    { "SDK",                        "SDK" },
    { "Application",                "Aplicativo" },
    { "Security",                   "Segurança" },
    { "WiFi Module",                "Módulo WiFi" },
    { "N/A",                        "N/D" },

    // Device info page
    { "Model",                      "Modelo" },
    { "Serial No",                  "Nº de série" },
    { "IMEI",                       "IMEI" },
    { "Battery",                    "Bateria" },
    { "Voltage",                    "Tensão" },
    { "Storage",                    "Armazenamento" },
    { "Charging",                   "Carregando" },
    { "Tamper",                     "Violação" },
    { "Normal",                     "Normal" },
    { "Triggered",                  "Acionada" },

    // Tamper alert page
    { "TAMPER ALERT",               "ALERTA DE VIOLAÇÃO" },
    { "Tamper detected!",           "Violação detectada!" },
    { "Tamper Code",                "Código de violação" },
    { "Check the device enclosure for tampering.",
      "Verifique a carcaça do dispositivo." },
    { "Press OK or BACK to dismiss", "Pressione OK ou BACK para fechar" },
    { "Unlock",                     "Desbloquear" },

    // Date time page
    { "Year",                       "Ano" },
    { "Month",                      "Mês" },
    { "Day",                        "Dia" },
    { "Hour",                       "Hora" },
    { "Minute",                     "Minuto" },
    { "Second",                     "Segundo" },

    // Network page
    { "4G",                         "4G" },
    { "Status",                     "Status" },
    { "SSID",                       "SSID" },
    { "IP Address",                 "Endereço IP" },
    { "MAC Address",                "Endereço MAC" },
    { "No SIM",                     "Sem SIM" },
    { "No service",                 "Sem serviço" },
    { "Connected",                  "Conectado" },
    { "Connecting",                 "Conectando" },
    { "Disconnected",               "Desconectado" },
    { "Unknown",                    "Desconhecido" },
    { "Switching...",               "Alternando..." },
    { "Set mode failed",            "Falha ao definir modo" },
    { "Reconnecting...",            "Reconectando..." },
    { "Reconnect WiFi",             "Reconectar WiFi" },
    { "WiFi AP List",               "Lista de AP WiFi" },
    { "WiFi Details",               "Detalhes WiFi" },
    { "Tap 4G / WiFi to switch. Enter applies the focused item.",
      "Toque 4G / WiFi para alternar. ENTER aplica o item focado." },

    // WiFi AP page
    { "Scanning...",                "Procurando..." },
    { "Connecting to %.20s...",     "Conectando a %.20s..." },
    { "Password: %.24s",            "Senha: %.24s" },
    { "Connect failed",             "Falha na conexão" },
    { "Network forgotten",          "Rede esquecida" },
    { "Cancelled",                  "Cancelado" },
    { "CONNECTED",                  "CONECTADO" },
    { "No AP found",                "Nenhum AP encontrado" },
    { "Rescan",                     "Procurar novamente" },
    { "Forget",                     "Esquecer" },

    // WiFi info page
    { "Subnet Mask",                "Máscara de sub-rede" },
    { "Gateway",                    "Gateway" },
    { "DNS 1",                      "DNS 1" },
    { "DNS 2",                      "DNS 2" },

    // Print page
    { "Printer",                    "Impressora" },
    { "Result",                     "Resultado" },
    { "Print Grade",                "Qualidade de impressão" },
    { "Print Test Page",            "Imprimir página de teste" },
    { "Make sure paper is loaded before printing.", "Verifique se há papel antes de imprimir." },
    { "Idle",                       "Ocioso" },
    { "Printing...",                "Imprimindo..." },
    { "Completed",                  "Concluído" },
    { "OK",                         "OK" },
    { "Out of paper",               "Sem papel" },

    // TMS page
    { "TMS Status",                 "Status TMS" },
    { "GPRS Module",                "Módulo GPRS" },
    { "Security Chip",              "Chip de segurança" },
    { "File System",                "Sistema de arquivos" },
    { "Firmware",                   "Firmware" },
    { "Check for Update",           "Verificar atualização" },
    { "Server",                     "Servidor" },
    { "Requires network. Keep the device powered on.",
      "Requer rede. Mantenha o dispositivo ligado." },
    { "Ready",                      "Pronto" },
    { "Server host is empty",       "Host do servidor vazio" },
    { "Server saved",               "Servidor salvo" },
    { "TMS Server (host:port)",     "Servidor TMS (host:porta)" },
    { "Checking...",                "Verificando..." },

    // Scan page
    { "Camera open failed",         "Falha ao abrir a câmera" },
    { "Scanned",                    "Digitalizado" },
    { "Tap Rescan for another scan, bottom BACK to return",
      "Toque em Procurar novamente; BACK para voltar" },

    // Test page
    { "MSR OK",                     "MSR OK" },
    { "MSR read failed",            "Falha na leitura MSR" },
    { "ICC OK",                     "ICC OK" },
    { "Multiple cards",             "Vários cartões" },
    { "PICC OK",                    "PICC OK" },
    { "Reading...",                 "Lendo..." },
    { "MSR open failed",            "Falha ao abrir MSR" },
    { "Swipe card...",              "Passe o cartão..." },
    { "ICC open failed",            "Falha ao abrir ICC" },
    { "Insert IC card...",          "Insira o cartão IC..." },
    { "PICC open failed",           "Falha ao abrir PICC" },
    { "Tap card...",                "Aproxime o cartão..." },
    { "Select a card test",         "Selecione um teste de cartão" },
    { "1  Magnetic Stripe",         "1  Tarja magnética" },
    { "2  IC Card",                 "2  Cartão IC" },
    { "3  Contactless",             "3  Sem contato" },
    { "4  QR Scan",                 "4  Leitura QR" },
    { "5  E-Signature",             "5  Assinatura" },
    { "Stop",                       "Parar" },

    // Payment page
    { "Enter amount first",         "Insira o valor primeiro" },
    { "Enter amount",               "Insira o valor" },
    { "Reading card...",            "Lendo cartão..." },
    { "Multiple cards, remove one", "Vários cartões, remova um" },
    { "Card reader init failed",    "Falha ao iniciar o leitor" },
    { "Swipe / Insert / Tap card",  "Passe / Insira / Aproxime o cartão" },
    { "CLR",                        "LIM" },
    { "DEL",                        "APG" },
    { "PAY",                        "PAGAR" },
    { "Touch keypad to enter amount, PAY to start. BACK to exit",
      "Use o teclado para o valor; PAGAR para iniciar. BACK para sair." },

    // Signature page
    { "Sign in the box, then press Print", "Assine na caixa e pressione Imprimir" },
    { "Please sign first",          "Assine primeiro" },
    { "Printed",                    "Impresso" },
    { "Sign here",                  "Assine aqui" },
    { "Clear",                      "Limpar" },

    // Maintenance page
    { "Maintenance mode is running...", "Modo de manutenção em execução..." },
    { "Device is now in firmware download mode.",
      "O dispositivo está no modo de download de firmware." },
    { "Start the download tool on PC.", "Inicie a ferramenta de download no PC." },
    { "Starting maintenance mode...", "Iniciando modo de manutenção..." },
    { "Tap BACK to exit maintenance mode", "Toque em BACK para sair do modo de manutenção" },

    // Placeholder page
    { "Under Construction",         "Em construção" },
    { "Press BACK or tap the bottom bar to return",
      "Pressione BACK ou toque na barra inferior para voltar" },

    // EMV page
    { "PIN  - - - -",               "PIN  - - - -" },
    { "Processing online...",       "Processando online..." },
    { "Processing...",              "Processando..." },
    { "OFFLINE PIN (last try), 4-12 digits", "PIN OFFLINE (última tentativa), 4-12 dígitos" },
    { "OFFLINE PIN, 4-12 digits, empty OK = bypass",
      "PIN OFFLINE, 4-12 dígitos, vazio OK = ignorar" },
    { "MSR PIN, 4-12 digits",       "PIN MSR, 4-12 dígitos" },
    { "ONLINE PIN, 4-12 digits, empty OK = bypass",
      "PIN ONLINE, 4-12 dígitos, vazio OK = ignorar" },
    { "Enter 4-12 digits or leave empty", "Digite 4-12 dígitos ou deixe vazio" },
    { "APPROVED",                   "APROVADO" },
    { "DECLINED",                   "NEGADO" },
    { "Print OK",                   "Impressão OK" },
    { "Print failed",               "Falha na impressão" },
    { "4-12 digits",                "4-12 dígitos" },
    { "SELECT APPLICATION",         "SELECIONE O APLICATIVO" },
    { "Confirm",                    "Confirmar" },
    { "CARD",                       "CARTÃO" },
    { "EXPIRY",                     "VALIDADE" },
    { "APPLICATION",                "APLICATIVO" },
    { "EMV KERNEL",                 "KERNEL EMV" },
    { "Done",                       "Concluído" },
    { "Start EMV failed",           "Falha ao iniciar EMV" },

    // Status bar
    { "NO SIM",                     "SEM SIM" },
    { "No Svc",                     "Sem serviço" },
};

static const LIB_LANG_TABLE_T s_ptTable =
{
    s_ptEntries,
    (unsigned short)(sizeof(s_ptEntries) / sizeof(s_ptEntries[0])),
};

//-----------------------------------------------------------------------------
// Global variable
//-----------------------------------------------------------------------------
const LIB_LANG_DESC_T g_lang_pt =
{
    "Português",                /* pName     : displayed in the language page */
    "pt",                       /* pCode     : must be unique */
    1,                          /* ucParam   : value written to E_LANG_TYPE */
    0,                          /* ucRtl     : 0 - left to right */
    "AlibabaSans-Regular.ttf",  /* pFontName : same TTF as English */
    &s_ptTable,                 /* pTable    : translation table */
};
