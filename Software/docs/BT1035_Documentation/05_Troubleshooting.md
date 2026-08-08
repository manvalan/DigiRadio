# Diagnostica e Risoluzione Problemi

1. **Modulo non risponde**: Verificare i livelli logici RX/TX. Il modulo BT1035 richiede TX e RX stabili. Controllare se il Pin `SYS_CTRL` è alto.
2. **Audio distorto**: Verificare che l'impedenza di carico sui pin `SPK_P/N` sia conforme e che il volume digitale (`AT+SPKVOL`) non sia in clipping.
3. **Mancato Pairing**: Verificare `AT+PAIR=1`. Assicurarsi che il modulo non sia già connesso a un altro dispositivo.
4. **Terminazione Comandi**: Ogni comando deve terminare con `0x0D` (CR) e `0x0A` (LF).
