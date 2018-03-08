
set(BIN_SOURCE
	startup_samd11.c 
	system_samd11.c 
	clockController/main.cpp
	clockController/i2c.cpp
	clockController/flipdot.cpp
	Adafruit-GFX-Library/Adafruit_GFX.cpp
)

cm0_add_executable(ClockController ${BIN_SOURCE} ${BIN_HEADERS})

include_directories(
  ${PROJECT_SOURCE_DIR}/include
)



