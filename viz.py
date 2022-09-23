import numpy as np
import matplotlib.pyplot as plt

arr = np.fromfile("DEPTHBUFFER", dtype=np.float32)

print(np.min(arr), np.max(arr))

#plt.imshow(arr, cmap='gray', vmin=0, vmax=255)
#plt.show()
