from math import e, pi, sqrt

def gauss(x):
	return (1/sqrt(2*pi)) * (e ** (-(1/2) * x*x))

def r(i, j):
	return sqrt(i*i + j*j)

N = 1
S = 0

for i in range(-N, N + 1):
	for j in range(-N, N + 1):
		f = gauss(r(i, j))
		S += f
		print(f, end=" ")
	print("")

print(S)
