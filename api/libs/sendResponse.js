export default function sendResponse(res, statusCode = 200, error = false, msg = 'Proceso exitoso.', data = []){
    if(error){
        return res.status(statusCode).json({ error, msg });
    }else{
        return res.status(statusCode).json({ error, msg, data });
    }
}