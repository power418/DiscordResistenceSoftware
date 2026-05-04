import rpc;

int main() {
  rpc::load_dotenv();
  return rpc::app::run();
}
