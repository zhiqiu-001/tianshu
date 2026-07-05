function fetchStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            document.getElementById('status-content').innerHTML = JSON.stringify(data, null, 2);
        })
        .catch(error => console.error('Error:', error));
}

function fetchNodes() {
    fetch('/api/nodes')
        .then(response => response.json())
        .then(data => {
            document.getElementById('nodes-content').innerHTML = JSON.stringify(data, null, 2);
        })
        .catch(error => console.error('Error:', error));
}

fetchStatus();
fetchNodes();
setInterval(fetchStatus, 5000);
setInterval(fetchNodes, 5000);