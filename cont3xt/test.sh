curl -H "Content-Type: application/json" http://localhost:9200/cont3xt_links/_doc/links1 -d '
{
  "name": "Links 1",
  "creator": "awick",
  "viewRoles": ["role1"],
  "editRoles": ["role", "role2"],
  "links": [
    {
      "name": "foo2",
      "url": "http://www.example.com",
      "itype": "ip"
    },
    {
      "name": "foo2",
      "url": "http://www.example.com",
      "itype": "ip"
    }
  ]
}
'

curl -H "Content-Type: application/json" http://localhost:9200/cont3xt_links/_doc/links2 -d '
{
  "name": "Links 2",
  "creator": "awick",
  "viewRoles": ["role1"],
  "editRoles": ["role", "role2"],
  "links": [
    {
      "name": "foo2",
      "url": "http://www.example.com",
      "itype": "ip"
    },
    {
      "name": "foo2",
      "url": "http://www.example.com",
      "itype": "ip"
    }
  ]
}
'
