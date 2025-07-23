import dotenv from 'dotenv';
import { createPool } from "mysql2/promise";
// Obtiene la configuracion de las variables de entorno
dotenv.config();

export const connection = createPool({
    host: process.env.DB_HOST,
    user: process.env.DB_USER,
    password: process.env.DB_PASSWORD,
    port: process.env.DB_PORT,
    database: process.env.DB_NAME
});