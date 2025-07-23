import dotenv from "dotenv";
dotenv.config({path: "./config.env"});
import sendResponse from "../libs/sendResponse.js";

export const verifyToken = (req, res, next) => {
    // Obtener el api key medante la cabexera
    const apiKey = req.headers['x-api-key'];

    try{
        // Verificar y decodificar el token
        if(apiKey === process.env.API_KEY) {
            return next();
        }

        return sendResponse(res, 403, true, 'Token invalido');
    }catch(error){
        return sendResponse(res, 403, true, 'Token invalido');
    }
}