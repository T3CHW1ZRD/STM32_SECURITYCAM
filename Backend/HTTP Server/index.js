#!/usr/bin/env node
'use strict';
const { PrismaClient } = require('@prisma/client');
const prisma = new PrismaClient();

const port =(()=>{
    const args = process.argv;

    if(args.length !== 3){
        console.error("Usage: node index.js port not provided");
        process.exit(1);
    }

    const num = parseInt(args[2], 10);
    if(isNaN(num)){
        console.error("error: argument for port must be an integer");
        process.exit(1);
    }
    return num;
})();

const express = require("express");
const app = express();
app.use(express.json());

app.post("/sample", (req, res) => {

  console.log("Received data:", req.body);
  res.status(200).send({ message: "Data received successfully!" });
});

app.post("/device", async(req, res) => {
    //confirm whether the device already exists
    //create a new device model
    const deviceIp = req.headers['X-Client'];
    const device = await(prisma.device.findUnique({where: {id: deviceIp}}));
    if(!device){
        device = await prisma.device.create({
            data:{
                ip: deviceIp,
                temperature: 0,
                humidity: 0
            }
        });
        console.log("New device registered");
    }
    //return res.status(200).json({message})
});

//this endpoint redirects the request to correct deviceId 

app.post("/image", (req, res) =>{

});
app.post("/device/:deviceId/image", (req,res)=>{

});

app.post("/entry", (req, res) =>{

});
app.post("/device/:deviceId/entry", (req,res) =>{
    
})

const server = app.listen(port, "0.0.0.0", ()=>{
    console.log(`Server running on port ${port}`);
});


server.on('error', (err) =>{
    console.error(`cannot start server: ${err.message}`);
})