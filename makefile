CC=cl
CFLAGS=/nologo /W3 /MT
TARGET=autoexec.exe
SOURCE=autoexec.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) /Fe$(TARGET) $(SOURCE)

clean:
	del $(TARGET) *.obj 2>nul || true

.PHONY: clean