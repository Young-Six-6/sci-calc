const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('musicKeyboard', {
  listPorts: () => ipcRenderer.invoke('serial:list'),
  connect: (port) => ipcRenderer.invoke('serial:connect', port),
  disconnect: () => ipcRenderer.invoke('serial:disconnect'),
  saveMidi: (fileName, bytes) => ipcRenderer.invoke('file:save-midi', { fileName, bytes }),
  chooseMidi: () => ipcRenderer.invoke('midi:choose'),
  convertMidi: (options) => ipcRenderer.invoke('midi:convert', options),
  buildLesson: (options) => ipcRenderer.invoke('midi:lesson', options),
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
