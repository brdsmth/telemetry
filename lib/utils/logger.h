#ifndef LOGGER_H
#define LOGGER_H

void logln(String msg);
void logkv(const char* key, const char* value);
void logkv(const char* key, int value);

#endif