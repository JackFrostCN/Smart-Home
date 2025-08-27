import React, { useState, useEffect } from 'react';
import './App.css';

function App() {
  const [isOnline, setIsOnline] = useState(false);
  const [loading, setLoading] = useState(false);
  const [data, setData] = useState({
    indoor: { temperature: 0, humidity: 0 },
    outdoor: { temperature: 0, humidity: 0 },
    lightLevel: 0,
    motion: false,
    wifi: false,
    lastUpdate: new Date().toISOString(),
    devices: {
      fan: { status: false, manual: false },
      ac: { status: false, manual: false },
      light: { status: false, manual: false }
    }
  });

  const API_BASE_URL = 'http://localhost:5000/api';

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 3000);
    return () => clearInterval(interval);
  }, []);

  const updateConnectionStatus = (online) => setIsOnline(online);

  const fetchData = async () => {
    try {
      const response = await fetch(`${API_BASE_URL}/status`);
      if (response.ok) {
        const apiData = await response.json();
        setData(apiData);
        updateConnectionStatus(true);
      } else throw new Error('API request failed');
    } catch (error) {
      console.error('Failed to fetch data:', error);
      updateConnectionStatus(false);
    }
  };

  const controlDevice = async (deviceName, action) => {
    if (loading || !isOnline) return;
    setLoading(true);
    try {
      const response = await fetch(`${API_BASE_URL}/device/${deviceName}/${action}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
      });
      if (response.ok) {
        setTimeout(fetchData, 500);
      } else throw new Error('Device control failed');
    } catch (error) {
      console.error('Failed to control device:', error);
      alert(`Failed to control ${deviceName}. Please try again.`);
    } finally {
      setLoading(false);
    }
  };

  const formatLastUpdate = (dateString) => new Date(dateString).toLocaleTimeString();

  return (
    <div className="App">
      <ConnectionStatus isOnline={isOnline} />
      <div className="container">
        <Header wifiStatus={data.wifi} lastUpdate={formatLastUpdate(data.lastUpdate)} />
        <EnvironmentGrid 
          indoorTemp={data.indoor.temperature} 
          indoorHum={data.indoor.humidity} 
          outdoorTemp={data.outdoor.temperature} 
          outdoorHum={data.outdoor.humidity} 
        />
        <SensorsGrid lightLevel={data.lightLevel} motion={data.motion} />
        <DeviceControls devices={data.devices} controlDevice={controlDevice} loading={loading} />
      </div>
    </div>
  );
}

// Header Component
const Header = ({ wifiStatus, lastUpdate }) => (
  <header className="header">
    <div className="header-left">
      <svg className="header-icon" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6"></path>
      </svg>
      <h1 className="header-title">Smart Home Dashboard</h1>
    </div>
    <div className="header-right">
      <div className="wifi-status">
        <svg className={`wifi-icon ${wifiStatus ? 'wifi-connected' : 'wifi-disconnected'}`} fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M8.111 16.404a5.5 5.5 0 017.778 0M12 20h.01m-7.08-7.071c3.904-3.905 10.236-3.905 14.141 0M1.394 9.393c5.857-5.857 15.355-5.857 21.213 0"></path>
        </svg>
        <span>{wifiStatus ? 'Connected' : 'Offline'}</span>
      </div>
      <div className="last-update">
        Last update: <span>{lastUpdate}</span>
      </div>
    </div>
  </header>
);

// Environment Grid
const EnvironmentGrid = ({ indoorTemp, indoorHum, outdoorTemp, outdoorHum }) => (
  <div className="env-grid">
    <EnvironmentCard type="indoor" temperature={indoorTemp} humidity={indoorHum} />
    <EnvironmentCard type="outdoor" temperature={outdoorTemp} humidity={outdoorHum} />
  </div>
);

const EnvironmentCard = ({ type, temperature, humidity }) => (
  <div className="card">
    <div className={`card-header ${type}-header`}>
      {type === 'indoor' ? 'Indoor Environment' : 'Outdoor Environment'}
    </div>
    <div className="env-values">
      <EnvValue type="temperature" value={temperature} unit="°C" label="Temperature" />
      <EnvValue type="humidity" value={humidity} unit="%" label="Humidity" />
    </div>
  </div>
);

const EnvValue = ({ type, value, unit, label }) => (
  <div className="env-value">
    <div className="value-header">
      <span className="value-number">{value.toFixed(1)}{unit}</span>
    </div>
    <div className="value-label">{label}</div>
  </div>
);

// Sensors Grid
const SensorsGrid = ({ lightLevel, motion }) => (
  <div className="sensors-grid">
    <SensorCard type="light" value={lightLevel} unit="lux" status={lightLevel > 100} />
    <SensorCard type="motion" value={motion ? 'DETECTED' : 'CLEAR'} status={motion} />
  </div>
);

const SensorCard = ({ type, value, unit, status }) => (
  <div className="card sensor-card">
    <div className="card-header">{type === 'light' ? 'Light Level' : 'Motion Detection'}</div>
    <div className="sensor-value">{type === 'light' ? `${Math.round(value)} ${unit}` : value}</div>
    <div className={`sensor-indicator ${type === 'light' ? (status ? 'light-active' : 'light-inactive') : (status ? 'motion-detected' : 'motion-clear')}`}></div>
  </div>
);

// Device Controls
const DeviceControls = ({ devices, controlDevice, loading }) => (
  <div className="card">
    <div className="card-header">Device Controls</div>
    <div className="devices-grid">
      {['fan', 'ac', 'light'].map(name => (
        <DeviceCard key={name} name={name} device={devices[name]} controlDevice={controlDevice} loading={loading} />
      ))}
    </div>
  </div>
);

const DeviceCard = ({ name, device, controlDevice, loading }) => {
  const deviceNames = { fan: 'Fan', ac: 'Air Conditioner', light: 'Smart Light' };

  // Show On, Off, or Auto instead of Manual
  const modeLabel = device.manual ? (device.status ? 'On' : 'Off') : 'Auto';

  return (
    <div className={`device-card ${device.status ? 'device-active' : 'device-inactive'}`}>
      <div className="device-header">
        <div className="device-title">{deviceNames[name]}</div>
      </div>
      <div className="device-info">
        <div className="device-mode">Mode: <span>{modeLabel}</span></div>
        <div className="device-controls">
          <button className="btn btn-on" onClick={() => controlDevice(name, 'on')} disabled={loading}>Turn On</button>
          <button className="btn btn-off" onClick={() => controlDevice(name, 'off')} disabled={loading}>Turn Off</button>
          <button className="btn btn-auto" onClick={() => controlDevice(name, 'auto')} disabled={loading}>Auto</button>
        </div>
      </div>
    </div>
  );
};

// Connection Status
const ConnectionStatus = ({ isOnline }) => (
  <div className={`connection-status ${isOnline ? 'connection-online' : 'connection-offline'}`}>
    <span>{isOnline ? 'Online' : 'Offline'}</span>
  </div>
);

export default App;
