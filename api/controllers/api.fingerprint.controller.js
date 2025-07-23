import dotenv from "dotenv";
dotenv.config({path: "./config.env"});

import tls from 'tls';
import crypto from 'crypto';
import sendResponse from '../libs/sendResponse.js';

export const getFingerprint = async (req, res) => {
    try {
        const domain = 'localhost';
        const options = {
            host: domain,
            port: process.env.PORT_WEB,
            servername: domain, // SNI
            rejectUnauthorized: false,
        };

        const socket = tls.connect(options, () => {
            const cert = socket.getPeerCertificate(true);
            const der = cert.raw; // formato DER
            const sha1 = crypto.createHash('sha1').update(der).digest('hex');

            // Formatear como XX:XX:...
            const fingerprint = sha1.toUpperCase().match(/.{2}/g).join(':');

            socket.end();

            const response = {
                domain,
                fingerprint,
                expires: cert.valid_to
            }

            return sendResponse(res, 200, false, "Huella obtenida exitosamente", response);
        });

        socket.on('error', (err) => {
            console.error('Error al obtener la huella :', err.message);
            return sendResponse(res, 500, true, 'No se pudo obtener la huella.');
        });

    } catch (error) {
        console.error(error.message);
        return sendResponse(res, 500, true, 'Error interno del servidor.');
    }
}