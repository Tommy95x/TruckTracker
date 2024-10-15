#include "Defines.h"

//-------------------------------------------------
void writeFile(fs::FS &fs, const char * path, const char * message) 
{
  if(SD_DEBUG)
    Serial.printf("Writing file: %s\n", path);
  
  File file = fs.open(path, FILE_WRITE);
  if(!file) 
  {
    if(SD_DEBUG)
      Serial.println("Failed to open file for writing");
    return;
  }

  if(file.print(message)) 
  {
    if(SD_DEBUG)
      Serial.println("File written");
  } 
  else 
  {
    if(SD_DEBUG)
      Serial.println("Write failed");
  }

  file.close();
}

//-------------------------------------------------
void appendFile(fs::FS &fs, const char * path, const char * message) 
{
  if(SD_DEBUG)
    Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) 
  {
    if(SD_DEBUG)
      Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) 
  {
    if(SD_DEBUG)
      Serial.println("Message appended");
  } 
  else 
  {
    if(SD_DEBUG)
      Serial.println("Append failed");
  }
  file.close();
}
//-------------------------------------------------
void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
  if(SD_DEBUG)
    Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root)
  {
    if(SD_DEBUG)
      Serial.println("Failed to open directory");

    return;
  }

  if(!root.isDirectory())
  {
    if(SD_DEBUG)
      Srial.println("Not a directory");
      
    return;
  }

  File file = root.openNextFile();
  while(file)
  {
    if(file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());

      if(levels)
      {
          listDir(fs, file.name(), levels -1);
      }
    } 
    else 
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }

    file = root.openNextFile();
  }
}

//-------------------------------------------------
void createDir(fs::FS &fs, const char * path)
{
    Serial.printf("Creating Dir: %s\n", path);

    if(fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else 
    {
        Serial.println("mkdir failed");
    }
}

//-------------------------------------------------
void removeDir(fs::FS &fs, const char * path)
{
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path))
    {
      Serial.println("Dir removed");
    } 
    else 
    {
      Serial.println("rmdir failed");
    }
}

//-------------------------------------------------
void readFile(fs::FS &fs, const char * path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

//-------------------------------------------------
void renameFile(fs::FS &fs, const char * path1, const char * path2)
{
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

//-------------------------------------------------
void deleteFile(fs::FS &fs, const char * path)
{
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}