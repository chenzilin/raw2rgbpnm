
void qc_imag_bay2rgb8(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		unsigned int columns, unsigned int rows, int bpp);

void qc_imag_bay2rgb10(unsigned char *bay, int bay_line,
		unsigned char *rgb, int rgb_line,
		unsigned int columns, unsigned int rows, int bpp);

void qc_set_sharpness(int sharpness);

void qc_print_algorithms(void);

void qc_set_algorithm(const char *name);
