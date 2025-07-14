#!/usr/bin/env node
'use strict';

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

const server = app.listen(port, ()=>{
    console.log(`Server running on port ${port}`);
});

server.on('error', (err) =>{
    console.error(`cannot start server: ${err.message}`);
})