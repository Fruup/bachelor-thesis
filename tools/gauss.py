from math import e, pi, sqrt
import numpy as np
import matplotlib.pyplot as plt

def gauss(x):
	return (1/sqrt(2*pi)) * (e ** (-(1/2) * x*x))

def r(i, j):
	return sqrt(i*i + j*j)

def computeKernel(N):
	S = 0
	K = []

	for i in range(-N, N + 1):
		for j in range(-N, N + 1):
			f = gauss(r(i, j))
			S += f
			K.append(f)

	return [k/S for k in K]

N = 1
Kernel = computeKernel(N)

shape = (900, 1600)

def index(x, y):
	x_ = min(max(x, 0), shape[1] - 1)
	y_ = min(max(y, 0), shape[0] - 1)
	return y_ * shape[1] + x_

data = np.fromfile("DEPTH", dtype=np.float32)
out = []

for x in range(shape[1]):
	for y in range(shape[0]):
		s = 0

		for wx in range(-N, N + 1):
			for wy in range(-N, N + 1):
				k = Kernel[(wy + N) * (2*N+1) + (wx + N)]
				sample = data[index(x + wx, y + wy)]
				s += k * sample

		out.append(s)

out = np.array(out).reshape(shape)
data = np.array(data).reshape(shape)

# show
f, axarr = plt.subplots(1,2)
axarr[0].imshow(data)
axarr[1].imshow(out)
#plt.imshow(data-out)
# plt.imshow(data)
# plt.imshow(out)

plt.tight_layout()
plt.show()
