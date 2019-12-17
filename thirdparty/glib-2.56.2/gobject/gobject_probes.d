provider gobject {
	probe type__new(char *, unsigned long, unsigned long);
	probe object__new(void*, unsigned long);
	probe object__ref(void*, unsigned long, unsigned int);
	probe object__unref(void*, unsigned long, unsigned int);
	probe object__dispose(void*, unsigned long, unsigned int);
	probe object__dispose__end(void*, unsigned long, unsigned int);
	probe object__finalize(void*, unsigned long);
	probe object__finalize__end(void*, unsigned long);
	probe signal__new(unsigned int, char *, unsigned long);
	probe signal__emit(unsigned int, unsigned int, void *, unsigned long);
	probe signal__emit__end(unsigned int, unsigned int, void *, unsigned long);
};
