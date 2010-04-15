pdfgrep: main.cc
	g++ -o pdfgrep -g `pkg-config --cflags --libs poppler` main.cc
