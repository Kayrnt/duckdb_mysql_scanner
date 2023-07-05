use mysql_async::prelude::*;

struct MysqlConfiguration {
    url: String
}

impl MysqlConfiguration {
    fn new(url: String) -> Self {
        Self {
            url
        }
    }
}

/*#[tokio::main]
async fn main() -> Result<()> {
    let pool = mysql_async::Pool::new("mysql://root:root@localhost:3306")?;
    let mut conn = pool.get_conn().await?;
    let result = conn.query_iter("SELECT * FROM mysql.user").await?;
    let mut rows = result.collect::<Vec<_>>().await;
    println!("{:?}", rows);
    Ok(())
}*/