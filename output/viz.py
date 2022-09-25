import numpy as np
import matplotlib.pyplot as plt

shape = (450, 800)

data1 = np.fromfile("DEPTH", dtype=np.float32).reshape(shape)
data2 = np.fromfile("OUTPUT", dtype=np.float32).reshape(shape)

f, axarr = plt.subplots(1,2)
axarr[0].imshow(data1)
axarr[1].imshow(data2)

# data = np.fromfile("SAMPLES", dtype=np.float32)

# print(np.min(data))
# print(np.max(data))

# plt.plot(data)

plt.tight_layout()
plt.show()
