{
    "KDE-KIO-Protocols": {
        "ftp": {
            "Class": ":internet", 
            "Icon": "folder-remote", 
            "ProxiedBy": "http", 
            "X-DocPath": "kioslave5/ftp/index.html", 
            "copyFromFile": true, 
            "copyToFile": true, 
            "deleting": true, 
            "exec": "kf@QT_MAJOR_VERSION@/kio/kio_ftp", 
            "input": "none", 
            "listing": [
                "Name", 
                "Type", 
                "Size", 
                "Date", 
                "Access", 
                "Owner", 
                "Group", 
                "Link", 
                ""
            ], 
            "makedir": true, 
            "maxInstances": 20, 
            "maxInstancesPerHost": 5, 
            "moving": true, 
            "output": "filesystem", 
            "protocol": "ftp", 
            "reading": true, 
            "writing": true
        }
    }
}
