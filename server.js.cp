const express = require('express');
const jwt = require('jsonwebtoken');
const mysql = require('mysql');
const bodyParser = require('body-parser');
const bcrypt = require('bcrypt');

const app = express();

app.use(bodyParser());

app.use(function(req, res, next) {
  res.header("Access-Control-Allow-Origin", "*");
  res.header("Access-Control-Allow-Methods", "GET,PUT,POST,DELETE");
  res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
  next();
});

const server = require('http').Server(app);
const io = require('socket.io').listen(server);


const db = mysql.createConnection({
  host: 'localhost',
  user: 'broadseg',
  password: '1122334499',
  database: 'broadseg',
  //socketPath : '/Applications/MAMP/tmp/mysql/mysql.sock'

});

db.connect((err) => {
  if(err)
    throw err;
  console.log('mysql Connected ....');
});

// jwt secret key
const salt = bcrypt.genSaltSync(10);
const secret_key = bcrypt.hashSync('secretkey', salt);
console.log(`jwt secret key : ${secret_key}`);

// stocking refresh tokens
refreshTokens = [];

// Sockeet

io.on('connection', socket => {

  console.log('new user connected!');

  // User Authentication
  socket.on('auth', (token) => {
    jwt.verify(token, secret_key, {algorithm: 'HS256'}, (err, authData) => {
      if (err) {
        console.log('token incorrect!');
      }
      else {
        console.log('new user has been authorized!');
        socket.join('broadseg');
      }
    });
  });

  socket.on('disconnect', function(data) {
    if(socket.doorName) {
      console.log(`door : ${socket.doorName} is connected!`);
      updateDoorState(socket.doorName, 0)
    }
    console.log('user disconnected!');
  });

  // Door connecting / disconnecting
  socket.on('door-connected', (doorName) => {
    socket.doorName = doorName;
    console.log(`door : ${doorName} is connected!`);
    getCount(doorName);
    updateDoorState(doorName, 1);
  });

  function updateDoorState(doorName, state) {
    let sql = `UPDATE doors SET state='${state}' WHERE name='${doorName}'`;
    db.query(sql , (error, results) => {
      if (error) throw error;
      console.log(results);
      if(state) {
        addDoorNotification(`${doorName} connected`, 'link');
      } else {
        addDoorNotification(`${doorName} disconnected`, 'disconnect');
      }
      updateDoors(doorName);
    });
  }

  function addDoorNotification(message, icon) {
    let sql = `INSERT INTO notification (message, icon, time) VALUES ('${message}','${icon}', NOW())`;
    db.query(sql , (error, results) => {
      if (error) throw error;
      updateNotification(results.insertId);
    });
  }

   // get count door
  function getCount(doorName) {
    let sql = `SELECT count(id) as count FROM doors where name='${doorName}'`;
    db.query(sql, (error, results) => {
      if (error) throw error;
      console.log(results[0]['count']);
      if (results[0]['count']==0)
         addDoor(doorName);
    });
  }

  function addDoor(name) {
    let sql = `INSERT INTO doors (name, active, state) VALUES ('${name}', 1, 1)`;
    db.query(sql , (error, results) => {
      if (error) throw error;
    });
  }

  // Emit to front End Door state
  function updateDoors(doorName) {
    let sql = `SELECT * FROM doors where name='${doorName}'`;
    db.query(sql, (error, results) => {
      if (error) throw error;
      socket.to('broadseg').emit('update-door', results[0]);
    });
  }

  // Emit to front End Notification
  function updateNotification(id) {
    let sql = `SELECT * FROM notification where id='${id}'`;
    db.query(sql, (error, results) => {
      if (error) throw error;
      console.log(results);
      socket.to('broadseg').emit('get-notification', results[0]);
    });
  }

  // User Log
  socket.on('user-log', ({id, direction}) => {
    console.log(`user : ${id} is ${direction}!`);
    checkUserState(id, direction);
  });

  function checkUserState(id, direction) {
    let sql = `SELECT * FROM users where id='${id}'`;
    db.query(sql , (error, results) => {
      if (error) throw error;
      if (!results[0].state)
        direction = 'unauthorized';
      addUserLog(results[0].name, direction);
    });
  }

  function addUserLog(name, direction) {
    let sql = `INSERT INTO log (name, time, direction) VALUES ('${name}',NOW(),'${direction}')`;
    db.query(sql , (error, results) => {
      if (error) throw error;
      updateLog(results.insertId);
    });
  }

  // Emit to front End Logs
  function updateLog(id) {
    let sql = `SELECT * FROM log where id='${id}'`;
    db.query(sql, (error, results) => {
      if (error) throw error;
      console.log(results);
      socket.to('broadseg').emit('get-log', results[0]);
    });
  }
});

// Check id
app.post('/api/checkid', (req, res) => {
  if (req.body.code_card == undefined)
  {
    res.json('error');
  }
  else {
    console.log(req);
    let sql = `SELECT users.name FROM users, doors where code=${req.body.code_card} && users.state=1 && doors.name like '${req.body.device}' && doors.active=1`;
    db.query(sql, (error, user) => {
      if (error) throw error;
      console.log(user);
      if (user === undefined || user == 0) {
        res.json({'response':false, 'name':''});
      } else {
        // Save In Log
        let sql = `INSERT INTO log (name, time, direction, door) VALUES ('${user[0].name}', now(), '${req.body.direction}', '${req.body.device}')`;
        db.query(sql, (error, results) => {
          if (error) throw error;
          console.log(results);
          res.json({'response':true, 'name':user[0].name});
        });
      }
    });
  }
});

// Get users
app.get('/api/getusers', verifyToken, (req, res) => {
  let sql = `SELECT * FROM users`;
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Add user
app.post('/api/adduser', verifyToken, (req, res) => {
  let sql = `INSERT INTO users SET ?`;
  db.query(sql, req.body, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
  addModification('Add', req.body.code, req.body.area, 'Admin');
});

// Update user
app.put('/api/updateuser', verifyToken, (req, res) => {
  console.log(req.body);
  let sql = `UPDATE users SET ? WHERE id = ${req.body.id}`;
  addDeleteModification('Delete', req.body.id, 'Admin');
  db.query(sql, req.body, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
  addModification('Add', req.body.code, req.body.area, 'Admin');
});

// Delete user
app.delete('/api/deleteuser', verifyToken, (req, res) => {
  console.log(req.body);
  let sql = `DELETE FROM users WHERE id = ${req.body.id}`;
  addDeleteModification('Delete', req.body.id, 'Admin');
  db.query(sql, req.body, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Delete user url
app.get('/api/deleteuser/:id', verifyToken, (req, res) => {
  console.log(req.params.id);
  let sql = `DELETE FROM users WHERE id = ${req.params.id}`;
  addDeleteModification('Delete', req.params.id, 'Admin');
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Add Modification
function addModification(action, userCode, area, admin) {
  let sql = `INSERT INTO modification (action, userCode, area, admin) VALUES ('${action}', '${userCode}', '${area}', '${admin}')`;
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
  });
}

// Add Delete Modification
function addDeleteModification(action, userId, admin) {
  let sql = `SELECT code, area FROM users where id=${userId}`;
  db.query(sql, (error, user) => {
    if (error) throw error;
    console.log(user);
    sql = `INSERT INTO modification (action, userCode, area, admin) VALUES ('${action}', '${user[0].code}', '${user[0].area}', '${admin}')`;
    db.query(sql, (error, results) => {
      if (error) throw error;
      console.log(results);
    });
  });
}

// Get logs
app.get('/api/getlogs', verifyToken, (req, res) => {
  let sql = `SELECT * FROM log ORDER BY id DESC`;
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Get doors
app.get('/api/getdoors', verifyToken, (req, res) => {
  let sql = `SELECT * FROM doors`;
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Update doors
app.put('/api/updatedoor', verifyToken, (req, res) => {
  console.log(req.body);
  let sql = `UPDATE doors SET ? WHERE id = ${req.body.id}`;
  db.query(sql, req.body, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Get Notifications
app.get('/api/getnotification', verifyToken, (req, res) => {
  let sql = `SELECT * FROM notification WHERE state=1`;
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// read notification url
app.get('/api/readnotification/:id', verifyToken, (req, res) => {
  console.log(req.params.id);
  let sql = `UPDATE notification SET state=0 WHERE id=${req.params.id}`;
  db.query(sql, (error, results) => {
    if (error) throw error;
    console.log(results);
    res.json(results);
  });
});

// Login
app.post('/api/login', async (req, res) => {
  console.log(req.body);
  let sql = `SELECT * FROM admin WHERE login='${req.body.login}'`;
  db.query(sql, (error, user) => {
    if (error) throw error;
    if (user === undefined || user == 0) {
      res.status(401).send('login incorrect!');
    } else {
      console.log(req.body);
      console.log(user[0].password);
      bcrypt.compare(req.body.password, user[0].password, function(err, result){
        if (result == true) {
          if (req.body.remember) {
            const token = jwt.sign({id: user[0].id, login: user[0].login}, secret_key, {algorithm: 'HS256', expiresIn: '10m'});
            const salt = bcrypt.genSaltSync(10);
            const refreshToken = bcrypt.hashSync(user[0].login, salt);
            refreshTokens[refreshToken] = user[0].login;
            res.json({token, refreshToken});
          }
          else {
            const token = jwt.sign({id: user[0].id, login: user[0].login}, secret_key, {algorithm: 'HS256', expiresIn: '1d'});
            res.json({token});
          }
        }
        else {
          res.status(401).send('password incorrect!');
        }
      });
    }
  });
});

// Refresh Login
app.post('/api/login/refresh', async (req, res) => {
  const bearerHeader = req.headers['authorization'];
  const refreshToken = req.body.refreshToken;
  if (typeof bearerHeader !== 'undefined') {
    const bearerToken = bearerHeader.split(' ')[1];
    const decoded = jwt.decode(bearerToken, {complete: true});
    console.log(decoded);
    if (refreshToken in refreshTokens) {
      const token = jwt.sign({id: decoded.payload.id, login: decoded.payload.login}, secret_key, {algorithm: 'HS256', expiresIn: '10m'});
      res.json({token});
    }
    else
      res.status(401).send('refresh token incorrect!');
  } else {
    return res.status(401).send('token incorrect!');
  }
});

// Logout
app.post('/api/logout', async (req, res) => {
  const refreshToken = req.body.refreshToken;
  if (refreshToken in refreshTokens)
    delete refreshTokens[refreshToken];
  res.send(204)
});

// Verifying Token Middleware
function verifyToken(req, res, next) {
  const bearerHeader = req.headers['authorization'];
  if (typeof bearerHeader !== 'undefined') {
    const bearerToken = bearerHeader.split(' ')[1];
    req.token = bearerToken;
    if (bearerToken === 'null') {
      return res.status(401).send('incorrect token!');
    }
    jwt.verify(bearerToken, secret_key, {algorithm: 'HS256'}, (err, authData) => {
      if (err)
        return res.status(401).send('incorrect token!');
      next();
    });
  } else {
    return res.status(401).send('token not found!');
  }
}

server.listen('3000', () => {
  console.log('Server started on port 3000');
});
