import numpy as np
import matplotlib.pyplot as plt

data = np.fromfile("DEPTHBUFFER", dtype=np.float32).reshape((900, 1600))

plt.imshow(data)
plt.show()
