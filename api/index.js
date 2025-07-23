// Dependencias necesarias
import dotenv from "dotenv";
dotenv.config({path: "./config.env"});

import https from 'https';
import express from "express";
import helmet from "helmet";
import fs from 'fs';
import cors from 'cors';

// Rutas
import sensorMagnetico from "./routes/api.sensorMagnetico.routes.js";
import fingerprint from "./routes/api.fingerprint.routes.js";

// Funcion de respuesta
import sendResponse from "./libs/sendResponse.js";
import morgan from "morgan";

const app = express();
const PORT = process.env.PORT_WEB;

// Middlewares
app.use(helmet());
// Aumentamos el limite del payload
app.use(express.urlencoded({ extended : true, limit: '10mb', parameterLimit: 10000}));
app.use(express.json({ limit: '10mb' }));

app.use(morgan('dev'));

app.use(cors({
    origin: '*', // o mejor pon la URL específica de tu frontend
    methods: ['GET','POST','PUT','DELETE','OPTIONS'],
    allowedHeaders: ['Content-Type', 'x-api-key']
}));

// Rutas
app.use('/api', sensorMagnetico);
app.use('/api', fingerprint)

// Manejador de rutas no encontradas
app.use((req, res, next) => {
    sendResponse(res, 404, true, "Ruta no encontrada");
});

// Opciones SSL
const options = {
    key: fs.readFileSync('./certs/key.pem'),
    cert: fs.readFileSync('./certs/cert.pem'),
};

// Crear servidor HTTPS
https.createServer(options, app).listen(PORT, () => {
    console.log(`Servidor HTTPS corriendo en https://localhost:${PORT}`);
});