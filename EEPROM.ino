long EEPROM_readLong(int address)
{
  long b4 = EEPROM.read(address);
  long b3 = EEPROM.read(address + 1);
  long b2 = EEPROM.read(address + 2);
  long b1 = EEPROM.read(address + 3);
  
  return ((b4 << 0) & 0xFF) + ((b3 << 8) & 0xFFFF) + ((b2 << 16) & 0xFFFFFF) + ((b1 << 24) & 0xFFFFFFFF);
}

void EEPROM_writeLong(int address, long value)
{
      byte b4 = (value & 0xFF);
      byte b3 = ((value >> 8) & 0xFF);
      byte b2 = ((value >> 16) & 0xFF);
      byte b1 = ((value >> 24) & 0xFF);

      EEPROM.write(address, b4);
      EEPROM.write(address + 1, b3);
      EEPROM.write(address + 2, b2);
      EEPROM.write(address + 3, b1);
      EEPROM.commit();
}
