#ifndef ASSERT_H_INCLUDED__
#define ASSERT_H_INCLUDED__

#define ASSERTE(expr) \
	if (! (expr)) { \
fprintf (stderr, VTRR "ASSERT" VTO "! " #expr "\n"); \
		exit (-1); \
	}
#define BUG ASSERTE

#define ASSERT2(expr, fmt, a, b) \
	if (! (expr)) { \
fprintf (stderr, VTRR "ASSERT" VTO "! " #expr fmt "\n", a, b); \
		exit (-1); \
	}

#endif //ndef ASSERT_H_INCLUDED__
