const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('fallingKeys', {
  listPorts: () => ipcRenderer.invoke('serial:list'),
  connect: (port) => ipcRenderer.invoke('serial:connect', port),
  disconnect: () => ipcRenderer.invoke('serial:disconnect'),
  onSerialData: (callback) => {
    const listener = (_event, payload) => callback(payload);
    ipcRenderer.on('serial:data', listener);
    return () => ipcRenderer.removeListener('serial:data', listener);
  },
  onSerialStatus: (callback) => {
    const listener = (_event, payload) => callback(payload);
    ipcRenderer.on('serial:status', listener);
    return () => ipcRenderer.removeListener('serial:status', listener);
  },
});
