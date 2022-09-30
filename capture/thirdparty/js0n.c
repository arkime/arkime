// by jeremie miller - 2010
// public domain, contributions/improvements welcome via github at https://github.com/quartzjer/js0n

// gcc started warning for the init syntax used here, is not helpful so don't generate the spam, supressing the warning is really inconsistently supported across versions
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Winitializer-overrides"
#pragma GCC diagnostic ignored "-Woverride-init"

// opportunity to further optimize would be having different jump tables for higher depths
#define PUSH(i) if(depth == 1) prev = *out++ = ((cur+i) - js)
#define CAP(i) if(depth == 1) prev = *out++ = ((cur+i) - (js + prev) + 1)

// this makes a single pass across the json bytes, using each byte as an index into a jump table to build an index and transition state
int js0n(const unsigned char *js, unsigned int len, unsigned int *out, unsigned int olen) // ALW - Changed out to int from short
{
	unsigned int prev = 0, *oend; // ALW - Changed to int from short
	const unsigned char *cur, *end;
	int depth=0;
	int utf8_remain=0;
	static void *gostruct[] = 
	{
		[0 ... 255] = &&l_bad,
		['\t'] = &&l_loop, [' '] = &&l_loop, ['\r'] = &&l_loop, ['\n'] = &&l_loop,
		['"'] = &&l_qup,
		[':'] = &&l_loop,[','] = &&l_loop,
		['['] = &&l_up, [']'] = &&l_down, // tracking [] and {} individually would allow fuller validation but is really messy
		['{'] = &&l_up, ['}'] = &&l_down,
		['-'] = &&l_bare, [48 ... 57] = &&l_bare, // 0-9
		['t'] = &&l_bare, ['f'] = &&l_bare, ['n'] = &&l_bare // true, false, null
	};
	static void *gobare[] = 
	{
		[0 ... 31] = &&l_bad,
		[32 ... 126] = &&l_loop, // could be more pedantic/validation-checking
		['\t'] = &&l_unbare, [' '] = &&l_unbare, ['\r'] = &&l_unbare, ['\n'] = &&l_unbare,
		[','] = &&l_unbare, [']'] = &&l_unbare, ['}'] = &&l_unbare,
		[127 ... 255] = &&l_bad
	};
	static void *gostring[] = 
	{
		[0 ... 31] = &&l_bad, // ALW - removed 127 logic
		[32 ... 127] = &&l_loop, // ALW - changed to 127
		['\\'] = &&l_esc, ['"'] = &&l_qdown,
		[128 ... 191] = &&l_bad,
		[192 ... 223] = &&l_utf8_2,
		[224 ... 239] = &&l_utf8_3,
		[240 ... 247] = &&l_utf8_4,
		[248 ... 255] = &&l_bad
	};
	static void *goutf8_continue[] =
	{
		[0 ... 127] = &&l_bad,
		[128 ... 191] = &&l_utf_continue,
		[192 ... 255] = &&l_bad
	};
	static void *goesc[] = 
	{
		[0 ... 255] = &&l_bad,
		['"'] = &&l_unesc, ['\\'] = &&l_unesc, ['/'] = &&l_unesc, ['b'] = &&l_unesc,
		['f'] = &&l_unesc, ['n'] = &&l_unesc, ['r'] = &&l_unesc, ['t'] = &&l_unesc, ['u'] = &&l_unesc
	};
	void **go = gostruct;
	
	for(cur=js,end=js+len,oend=out+olen; cur<end && out<oend; cur++)
	{
			goto *go[*cur];
			l_loop:;
	}
	
	if(out < oend) *out = 0;
	return depth; // 0 if successful full parse, >0 for incomplete data
	
	l_bad:
		return cur - js + 1; // ALW - Return where the error happen
	
	l_up:
		PUSH(0);
		++depth;
		goto l_loop;

	l_down:
		--depth;
		CAP(0);
		goto l_loop;

	l_qup:
		PUSH(1);
		go=gostring;
		goto l_loop;

	l_qdown:
		CAP(-1);
		go=gostruct;
		goto l_loop;
		
	l_esc:
		go = goesc;
		goto l_loop;
		
	l_unesc:
		go = gostring;
		goto l_loop;

	l_bare:
		PUSH(0);
		go = gobare;
		goto l_loop;

	l_unbare:
		CAP(-1);
		go = gostruct;
		goto *go[*cur];

	l_utf8_2:
		go = goutf8_continue;
		utf8_remain = 1;
		goto l_loop;

	l_utf8_3:
		go = goutf8_continue;
		utf8_remain = 2;
		goto l_loop;

	l_utf8_4:
		go = goutf8_continue;
		utf8_remain = 3;
		goto l_loop;

	l_utf_continue:
		if (!--utf8_remain)
			go=gostring;
		goto l_loop;

}

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif
