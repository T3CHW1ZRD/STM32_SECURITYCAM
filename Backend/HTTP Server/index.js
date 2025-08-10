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
const fs = require('fs');
const path = require('path');

app.use(express.json());
app.use('/uploads', express.static(path.join(__dirname, 'uploads')));
app.post("/sample", (req, res) => {

  console.log("Received data:", req.body);
  res.status(200).send({ message: "Data received successfully!" });
});

app.post("/device", async(req, res) => {
    //confirm whether the device already exists
    //create a new device model
    const deviceIp = req.headers['x-client'];
    
    const device = await(prisma.device.findUnique({where: {ip: deviceIp}}));
    if(!device){
        const newDevice = await prisma.device.create({
            data:{
                ip: deviceIp,
                temperature: 0,
                humidity: 0
            }
        });
        console.log("New device registered");
    }
    res.status(200).json('Registration complete');
});

//this endpoint redirects the request to correct deviceId 

app.post("/device/image", express.raw({ type: 'application/octet-stream', limit: '10mb' }), async(req, res) =>{
    try{
        const deviceIp = req.headers['x-client'];
        const device = await(prisma.device.findUnique({where: {ip: deviceIp}}));
        if(device){
            const deviceId = device.id;
            const timestamp = parseInt(req.headers['x-timestamp']);
            const image = req.body;

            const filename = `image_${Date.now()}.jpg`;
            const filepath = path.join(__dirname, 'uploads', filename);

            fs.writeFileSync(filepath, image);
            const newImage = await prisma.image.create({
                data:{
                    timestamp: new Date(),
                    imageURL: `/uploads/${filename}`,
                    device: {connect:{id: deviceId}}

                }
            });
            res.status(200).json('Registration complete')
        }
        else
            console.log("Device not Registered with server");
    }
    catch(err){
        console.log(err.message);
    }
});

app.get("/device", async(req, res)=>{
    try{
        const devices = await prisma.device.findMany();
        return res.status(200).json(devices);
    }
    catch(err){
        console.log(err.message);
        res.status(500).json({error: "Failed to get devices"});
    }
})

app.get("/device/:deviceId/Images", async(req, res) =>{
    try{
        const deviceId = parseInt(req.params.deviceId);
        const images = await prisma.image.findMany({where: {deviceId}});
        res.status(200).json(images);
    }
    catch(err){
        console.log(err.message);
        res.status(500).json({error:"Failed to get images"});
    }
});


const server = app.listen(port, "0.0.0.0", ()=>{
    console.log(`Server running on port ${port}`);
});


server.on('error', (err) =>{
    console.error(`cannot start server: ${err.message}`);
})