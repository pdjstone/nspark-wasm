
let worker = new Worker('nspark-worker.js');
let nextTid = 0;
let archiveQueues = {};

class AsyncQueue {

	constructor(doFirst = null) {
		this.buffer = [];
    this.resolve = null;
    this.closed = false;
    this.doFirst = doFirst;
  }
  
  push(item) {
      if (this.resolve) {
      	this.resolve(item);
        this.resolve = null;
      } else {
      	this.buffer.push(item);
      }
  }
  
  close() {
    	this.closed = true;
      if (this.resolve)
      	this.resolve();
  }
    
  [Symbol.asyncIterator]() {
  	 let self = this;
     return {
        async next() { 
          if (self.doFirst) {
            await self.doFirst();
            self.doFirst = null;
          }
          let item = null;
          if (self.buffer.length > 0) {
            console.log('take from buffer');
          	item = self.buffer.shift();
          } else { 
          	item = await new Promise(resolve => {
            	if (self.resolve)
              	console.warning('resolve should be null');
            	self.resolve = resolve;
          	});
          }
          if (!self.closed) {
            return { done: false, value: item };
          } else {
            return { done: true };
          }
        } 
      }
  }
}


worker.onmessage = function(msg) {
    let tid = msg.data.tid;
    if (!archiveQueues[tid]) {
      console.error('no queue for tid ' + tid);
    } else {
      let q = archiveQueues[tid];
      if (msg.data.hasOwnProperty('done')) {
        q.close();
      } else {
        q.push(msg.data);
      }
    }
}


async function fetchArchiveUrl(url) {
  let response = await fetch(url, {mode:'cors'});
  let buf = await response.arrayBuffer();
  return buf;
}

export function unarchiveUrl(url) {
    console.log('unarchive url: ' + url);
  
    let q = new AsyncQueue(async () => {
      let buf = await fetchArchiveUrl(url);
      let tid = nextTid++;
      archiveQueues[tid] = q;
      worker.postMessage({'buf': buf, 'tid': tid}, [buf]);
    });
   
    return q;
}
